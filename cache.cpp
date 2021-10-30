#include "cache.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

size_t log2(size_t num) {
  assert(num != 0);
  size_t res = 0;
  while (num > 1) {
    assert((num & 1) == 0);
    num >>= 1;
    res++;
  }
  return res;
}

Cache::Cache(size_t block_size, size_t assoc,
             ReplacementAlgorithm replacement_algo,
             WayPredictionAlgorithm way_prediction_algo,
             WriteHitPolicy hit_policy, WriteMissPolicy miss_policy) {
  this->block_size = block_size;
  this->assoc = assoc;
  this->replacement_algo = replacement_algo;
  this->way_prediction_algo = way_prediction_algo;
  this->hit_policy = hit_policy;
  this->miss_policy = miss_policy;

  this->num_set = cache_size / block_size / assoc;
  this->block_size_lg2 = log2(this->block_size);
  this->assoc_lg2 = log2(this->assoc);
  this->num_set_lg2 = log2(this->num_set);
  this->tag_width = 64 - this->num_set_lg2 - this->block_size_lg2;

  this->all_cachelines.resize(this->num_set * this->assoc, CacheLine());
  this->num_hit = 0;
  this->num_miss = 0;
  this->num_way_prediction_first_hit = 0;
}

Cache::~Cache() {}

std::vector<Trace> readTrace(FILE *fp) {
  char buffer[1024];
  std::vector<Trace> res;
  size_t num_r = 0, num_w = 0;
  char temp;
  while (fgets(buffer, sizeof(buffer), fp) != NULL) {
    size_t len = strlen(buffer);
    if (len > 0) {
      Trace trace;
      if (buffer[0] == 'r') {
        trace.kind = Kind::Read;
        sscanf(buffer, "%c%llx", &temp, &trace.addr);
        num_r++;
      } else if (buffer[0] == 'w') {
        trace.kind = Kind::Write;
        sscanf(buffer, "%c%llx", &temp, &trace.addr);
        num_w++;
      } else {
        printf("Invalid trace\n");
        exit(1);
      }
      res.push_back(trace);
    }
  }

  printf("Read %ld entries: %ld reads, %ld writes\n", res.size(), num_r, num_w);
  return res;
}

void Cache::run(const std::vector<Trace> &traces, FILE *trace, FILE *info) {
  this->trace = trace;
  this->info = info;

  fprintf(info, "Block size: %ld Bytes\n", this->block_size);
  fprintf(info, "Assoc: %ld-way\n", this->assoc);
  fprintf(info, "Number of cacheline: %ld\n", this->num_set * this->assoc);
  fprintf(info, "Tag width: %ld\n", this->tag_width);
  fprintf(info, "Index width: %ld\n", this->num_set_lg2);
  fprintf(info, "Offset width: %ld\n", this->block_size_lg2);

  if (hit_policy == WriteHitPolicy::Writeback) {
    fprintf(info, "Write Hit Policy: Writeback\n");
  } else {
    fprintf(info, "Write Hit Policy: Writethrough\n");
  }
  if (miss_policy == WriteMissPolicy::WriteAllocate) {
    fprintf(info, "Write Miss Policy: Write Allocate\n");
  } else {
    fprintf(info, "Write Miss Policy: Write Non-allocate\n");
  }

  if (replacement_algo == ReplacementAlgorithm::LRU) {
    fprintf(info, "Replacement Algorithm: LRU\n");
    this->lru_state.resize(num_set, LRUState(this->assoc_lg2));
  } else {
    printf("Unknown replacement algorithm\n");
    exit(1);
  }

  if (way_prediction_algo == WayPredictionAlgorithm::None) {
    fprintf(info, "Way Prediction Algorithm: None\n");
  } else if (way_prediction_algo == WayPredictionAlgorithm::MRU) {
    fprintf(info, "Way Prediction Algorithm: MRU\n");
    this->mru_state.resize(num_set, 0);
  } else {
    printf("Unknown way prediction algorithm\n");
    exit(1);
  }

  for (const Trace &access : traces) {
    if (access.kind == Kind::Read) {
      read(access);
    } else if (access.kind == Kind::Write) {
      write(access);
    }
  }

  fprintf(info, "Memory access: %ld\n", traces.size());
  fprintf(info, "Hit: %ld\n", this->num_hit);
  fprintf(info, "Hit Rate: %.2f%%\n", 100.0 * (this->num_hit) / traces.size());
  fprintf(info, "Miss: %ld\n", this->num_miss);
  fprintf(info, "Miss Rate: %.2f%%\n",
          100.0 * (this->num_miss) / traces.size());
  if (way_prediction_algo != WayPredictionAlgorithm::None) {
    fprintf(info, "Way Prediction First Hit: %ld\n",
            num_way_prediction_first_hit);
    fprintf(info, "Way Prediction First Hit Rate: %.2f%%\n",
            100.0 * num_way_prediction_first_hit / num_hit);
    fprintf(info, "Way Prediction Non-First Hit: %ld\n",
            num_hit - num_way_prediction_first_hit);
    fprintf(info, "Way Prediction Non-First Hit Rate: %.2f%%\n",
            100.0 * (num_hit - num_way_prediction_first_hit) / num_hit);
  }
  assert(this->num_hit + this->num_miss == traces.size());
}

void Cache::read(const Trace &access) {
  uint64_t tag = (access.addr >> num_set_lg2) >> block_size_lg2;
  uint64_t index = (access.addr >> block_size_lg2) & (num_set - 1);
  CacheLine *cacheline = &all_cachelines[index * assoc];

  // find matching cacheline
  for (size_t i = 0; i < assoc; i++) {
    if (cacheline[i].get_valid() && cacheline[i].get_tag() == tag) {
      // hit
      fprintf(trace, "Hit at 0x%08llx\n", access.addr);
      num_hit++;

      // update state
      if (replacement_algo == ReplacementAlgorithm::LRU) {
        this->lru_state[index].hit(i);
      }

      if (way_prediction_algo == WayPredictionAlgorithm::MRU) {
        if (i == this->mru_state[index]) {
          num_way_prediction_first_hit++;
        }
        this->mru_state[index] = i;
      }
      return;
    }
  }

  // miss
  fprintf(trace, "Miss at 0x%08llx\n", access.addr);
  num_miss++;

  size_t victim = 0;
  if (replacement_algo == ReplacementAlgorithm::LRU) {
    // get victim from last element
    victim = this->lru_state[index].victim();
    // hit it to put it on top
    this->lru_state[index].hit(victim);
  }

  cacheline[victim].set_valid(true);
  cacheline[victim].set_dirty(false);
  cacheline[victim].set_tag(tag);
}

void Cache::write(const Trace &access) {
  uint64_t tag = (access.addr >> num_set_lg2) >> block_size_lg2;
  uint64_t index = (access.addr >> block_size_lg2) & (num_set - 1);
  CacheLine *cacheline = &all_cachelines[index * assoc];

  // find matching cacheline
  for (size_t i = 0; i < assoc; i++) {
    if (cacheline[i].get_valid() && cacheline[i].get_tag() == tag) {
      // hit
      if (hit_policy == WriteHitPolicy::Writethrough) {
        // write through
        // do nothing because we don't store data
      } else {
        // write back
        cacheline[i].set_dirty(true);
      }

      // avoid code duplication
      read(access);
      return;
    }
  }

  // miss
  if (miss_policy == WriteMissPolicy::WriteNonAllocate) {
    // write non-allocate
    // just write, do nothing for no data
    fprintf(trace, "Miss at 0x%08llx\n", access.addr);
    num_miss++;
  } else {
    // write allocate
    read(access);
    if (hit_policy == WriteHitPolicy::Writethrough) {
      // write through
      // do nothing because we don't care about data
    } else {
      // write back
      for (size_t i = 0; i < assoc; i++) {
        if (cacheline[i].get_valid() && cacheline[i].get_tag() == tag) {
          cacheline[i].set_dirty(true);
          return;
        }
      }
    }
  }
}

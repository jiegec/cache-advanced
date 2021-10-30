#ifndef _CACHE_H_
#define _CACHE_H_

// for assert in release mode
#undef NDEBUG
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <vector>

enum Kind { Read, Write };
enum Algorithm { LRU };
enum WriteHitPolicy { Writethrough, Writeback };
enum WriteMissPolicy { WriteAllocate, WriteNonAllocate };

struct Trace {
  Kind kind;
  uint64_t addr;
};

// 512 KB
const size_t cache_size = 512 * 1024;

struct CacheLine {
  // metadata:
  bool dirty = false;
  bool valid = false;
  uint64_t tag = 0;

  uint64_t get_dirty() { return dirty; }
  void set_dirty(bool dirty) { this->dirty = dirty; }
  uint64_t get_valid() { return valid; }
  void set_valid(bool valid) { this->valid = valid; }
  uint64_t get_tag() { return tag; }
  void set_tag(uint64_t tag) { this->tag = tag; }
};

struct LRUState {
  std::vector<uint32_t> array;
  // n-ways
  int n;

  uint64_t victim() { return array.back(); }

  // move i to the first of the array
  void hit(uint64_t i) {
    uint64_t last = i;
    for (size_t j = 0; j < n; j++) {
      uint64_t item = array[j];
      array[j] = last;
      last = item;
      if (item == i) {
        // found
        return;
      }
    }
    // unreachable
    assert(false);
  }

  LRUState(size_t assoc_lg2) {
    n = 1 << assoc_lg2;
    // initialize to: n-1, n-2, ..., 0
    for (size_t i = 0; i < n; i++) {
      array.push_back(n - i - 1);
    }
  }
};

class Cache {
private:
  // cache parameters
  size_t block_size;
  size_t assoc;

  // cache constants
  size_t num_set;        // cache_size / block_size / assoc
  size_t block_size_lg2; // log2(block_size)
  size_t assoc_lg2;      // log2(assoc)
  size_t num_set_lg2;    // log2(num_set)
  size_t tag_width;      // 64 - num_set_lg2 - block_size_lg2

  // algorithm and policy
  Algorithm algo;
  WriteHitPolicy hit_policy;
  WriteMissPolicy miss_policy;

  // statistics and output
  size_t num_hit;
  size_t num_miss;
  FILE *trace;
  FILE *info;

  // these are stored in hardware
  // num_set * assoc elements
  std::vector<CacheLine> all_cachelines;

  // LRU specific: num_set elements
  std::vector<LRUState> lru_state;

  void read(const Trace &access);
  void write(const Trace &access);

public:
  Cache(size_t block_size, size_t assoc, Algorithm algo,
        WriteHitPolicy hit_policy, WriteMissPolicy miss_policy);
  ~Cache();

  void run(const std::vector<Trace> &traces, FILE *trace, FILE *info);
};

std::vector<Trace> readTrace(FILE *fp);

#endif

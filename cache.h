#ifndef _CACHE_H_
#define _CACHE_H_

// for assert in release mode
#undef NDEBUG
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <vector>

enum Kind { Read, Write };
enum ReplacementAlgorithm { LRU };
enum WayPredictionAlgorithm { None, MRU, MultiColumn };
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

  // swap two blocks
  void swap(uint64_t a, uint64_t b) {
    assert(0 <= a && a < n);
    assert(0 <= b && b < n);
    assert(a != b);
    for (size_t j = 0; j < n; j++) {
      if (array[j] == a) {
        array[j] = b;
      } else if (array[j] == b) {
        array[j] = a;
      }
    }
  }

  LRUState(size_t assoc_lg2) {
    n = 1 << assoc_lg2;
    // initialize to: n-1, n-2, ..., 0
    for (size_t i = 0; i < n; i++) {
      array.push_back(n - i - 1);
    }
  }
};

struct MultiColumnState {
  std::vector<uint32_t> bit_vec;
  // n-ways
  int n;

  MultiColumnState(size_t assoc_lg2) {
    n = 1 << assoc_lg2;
    // initialize to: all zeros
    bit_vec.resize(n);
  }
};

struct VictimCacheState {
  std::vector<CacheLine> data;
  std::vector<uint32_t> lru;
  // n-ways
  int n;

  // move i to the first of lru
  void hit(uint64_t i) {
    uint64_t last = i;
    for (size_t j = 0; j < n; j++) {
      uint64_t item = lru[j];
      lru[j] = last;
      last = item;
      if (item == i) {
        // found
        return;
      }
    }
    // unreachable
    assert(false);
  }

  VictimCacheState(size_t victim_cache_size) {
    n = victim_cache_size;
    data.resize(n);
    // initialize to: n-1, n-2, ..., 0
    for (size_t i = 0; i < n; i++) {
      lru.push_back(n - i - 1);
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
  ReplacementAlgorithm replacement_algo;
  WayPredictionAlgorithm way_prediction_algo;
  size_t victim_cache_size; // size=0 means disabled
  WriteHitPolicy hit_policy;
  WriteMissPolicy miss_policy;

  // statistics and output
  size_t num_hit;
  size_t num_miss;
  size_t num_way_prediction_first_hit;
  size_t num_multi_column_bit_vector_search_length;
  FILE *trace;
  FILE *info;

  // these are stored in hardware
  // num_set * assoc elements
  std::vector<CacheLine> all_cachelines;

  // LRU specific: num_set elements
  std::vector<LRUState> lru_state;

  // MRU specific: num_set elements
  std::vector<uint32_t> mru_state;

  // Multi Column specific: num_set elements
  std::vector<MultiColumnState> multi_column_state;

  // Victim Cache specific: victim_cache_size elements
  VictimCacheState victim_cache_state;

  void read(const Trace &access);
  void write(const Trace &access);

public:
  Cache(size_t block_size, size_t assoc, ReplacementAlgorithm algo,
        WayPredictionAlgorithm way_prediction_algo, size_t victim_cache_size,
        WriteHitPolicy hit_policy, WriteMissPolicy miss_policy);
  ~Cache();

  void run(const std::vector<Trace> &traces, FILE *trace, FILE *info);
};

std::vector<Trace> readTrace(FILE *fp);

#endif

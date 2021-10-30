#include "cache.h"
#include <assert.h>
#include <thread>

void run(std::vector<Trace> &traces, std::vector<std::thread> &threads,
         char *argv[], int task, size_t block_size,
         ReplacementAlgorithm replacement_algo,
         WayPredictionAlgorithm way_prediction_algo, WriteHitPolicy hit,
         WriteMissPolicy miss, size_t assoc) {
  threads.push_back(std::thread([=] {
    char buffer[1024];
    Cache cache(block_size, assoc, replacement_algo, way_prediction_algo, hit,
                miss);

    sprintf(buffer, "%s_task%d_%zu_%zu.trace", argv[1], task, block_size,
            assoc);
    printf("Writing to %s\n", buffer);
    FILE *trace = fopen(buffer, "w");
    assert(trace);

    sprintf(buffer, "%s_task%d_%zu_%zu.info", argv[1], task, block_size, assoc);
    FILE *info = fopen(buffer, "w");
    assert(info);
    cache.run(traces, trace, info);
    fclose(trace);
    fclose(info);
  }));
}

int main(int argc, char *argv[]) {
  if (argc != 2) {
    printf("Usage: cache [trace_file]\n");
    return 1;
  }

  FILE *fp = fopen(argv[1], "r");
  if (fp == NULL) {
    perror("unable to open trace file");
    return 1;
  }

  std::vector<Trace> traces = readTrace(fp);

  std::vector<std::thread> threads;

  // default settings
  size_t block_size = 64;
  ReplacementAlgorithm replacement_algo = ReplacementAlgorithm::LRU;
  WayPredictionAlgorithm way_prediction_algo = WayPredictionAlgorithm::None;
  WriteHitPolicy hit = WriteHitPolicy::Writeback;
  WriteMissPolicy miss = WriteMissPolicy::WriteAllocate;

  // task 1
  run(traces, threads, argv, 1, block_size, replacement_algo,
      way_prediction_algo, hit, miss, 1);

  // task 2
  // assoc = 2,4,8,16
  for (auto assoc : {
           (size_t)2,
           (size_t)4,
           (size_t)8,
           (size_t)16,
       }) {
    run(traces, threads, argv, 2, block_size, replacement_algo,
        way_prediction_algo, hit, miss, assoc);
  }

  // task 3
  // assoc = 2,4,8,16
  // way prediction algo = MRU
  for (auto assoc : {
           (size_t)2,
           (size_t)4,
           (size_t)8,
           (size_t)16,
       }) {
    run(traces, threads, argv, 3, block_size, replacement_algo,
        WayPredictionAlgorithm::MRU, hit, miss, assoc);
  }

  // task 4
  // assoc = 2,4,8,16
  // way prediction algo = MultiColumn
  for (auto assoc : {
           (size_t)2,
           (size_t)4,
           (size_t)8,
           (size_t)16,
       }) {
    run(traces, threads, argv, 4, block_size, replacement_algo,
        WayPredictionAlgorithm::MultiColumn, hit, miss, assoc);
  }

  for (auto &thread : threads) {
    thread.join();
  }

  fclose(fp);
  return 0;
}
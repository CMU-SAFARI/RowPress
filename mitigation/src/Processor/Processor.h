#pragma once

#include <iostream>
#include <vector>
#include <fstream>
#include <string>
#include <ctype.h>
#include <functional>

#include "Config.h"
#include "Cache.h"
#include "Core.h"
#include "Memory.h"
#include "Request.h"
#include "Statistics.h"


namespace Ramulator 
{
  class Core;

  class Processor 
  {
    private:
      float core_tick_period;

    public:
      std::vector<std::unique_ptr<Core>> cores;
      std::shared_ptr<CacheSystem> cachesys = nullptr;
      Cache* llc = nullptr;

      std::vector<double> ipcs;
      double ipc = 0;

      int l3_size = 1 << 23;
      int l3_assoc = 1 << 3;
      int l3_blocksz = 1 << 6;
      int mshr_per_bank = 16;

      // When early_exit is true, the simulation exits when the earliest trace finishes.
      bool early_exit;
      size_t warmup_insts;
      size_t expected_limit_insts;

      // When time_limit is nonzero, the simulation exits when the time_limit is reached.
      int time_limit;

      uint tick_unit;

      ScalarStat cpu_cycles;
      ScalarStat cpu_time;


    public:
      Processor(const YAML::Node& processor_config, const YAML::Node& memory_config, function<bool(Request)> send, MemoryBase* memory);

    public:
      void tick();
      void receive(Request& req);
      void reset_stats();
      bool finished();
      void finalize();
      bool has_reached_limit();
      uint64_t get_insts(); // the total number of instructions issued to all cores
  };

  Processor* make_processor(const YAML::Node& config, MemoryBase* memory);
}

#pragma once

#include <iostream>
#include <vector>
#include <fstream>
#include <string>
#include <functional>

#include "Config.h"
#include "Processor.h"
#include "Cache.h"
#include "Trace.h"
#include "Memory.h"
#include "Request.h"
#include "Statistics.h"

namespace Ramulator
{
  class Window 
  {
    public:
      int ipc = 4;
      int depth = 128;

    private:
      int load = 0;
      int head = 0;
      int tail = 0;
      std::vector<bool> ready_list;
      std::vector<long> addr_list;


    public:
      Window() : ready_list(depth), addr_list(depth, -1) {};

    public:
      bool is_full();
      bool is_empty();
      void insert(bool ready, long addr);
      long retire();
      void set_ready(long addr, int mask);
  };

  class Processor;

  class Core 
  {
    public:
      long clk = 0;
      long retired = 0;
      int id = 0;
      std::function<bool(Request)> send;

      Core(const YAML::Node& config, Processor* processor, int coreid,
          std::string& trace_fname,
          std::function<bool(Request)> send_next, Cache* llc,
          MemoryBase* memory);

      void tick();
      void receive(Request& req);
      void reset_stats();
      double calc_ipc();
      bool finished();
      bool has_reached_limit();
      uint64_t get_insts(); // the number of the instructions issued to the core
      std::function<void(Request&)> callback;

      int l1_size = 1 << 15;
      int l1_assoc = 1 << 3;
      int l1_blocksz = 1 << 6;
      int l1_mshr_num = 16;

      int l2_size = 1 << 18;
      int l2_assoc = 1 << 3;
      int l2_blocksz = 1 << 6;
      int l2_mshr_num = 16;
      std::vector<std::shared_ptr<Cache>> caches;
      Cache* llc;

      ScalarStat record_cycs;
      ScalarStat record_insts;

      // This is set true iff expected number of instructions has been executed or all instructions are executed.
      bool reached_limit = false;

    private:
      const Processor* processor;
      Trace trace;
      Window window;

      long bubble_cnt;
      long req_addr = -1;
      Request::Type req_type;
      bool more_reqs;
      long last = 0;

      Cache* first_level_cache = nullptr;

      ScalarStat memory_access_cycles;
      ScalarStat cpu_inst;
      MemoryBase* memory;
  };

};
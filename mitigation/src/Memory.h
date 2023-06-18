#pragma once

#include <vector>
#include <functional>
#include <cmath>
#include <cassert>
#include <tuple>
#include <random>

#include "bitmanip.h"
#include "Config.h"
#include "Translation.h"
#include "DRAM.h"
#include "Request.h"
#include "Controller.h"
#include "Statistics.h"

#include "Standards.h"

namespace Ramulator
{
  class MemoryBase
  {
    public:
      uint tick_unit;
      std::function<bool(Request)> send_fn;
      TranslationBase* translation;

    public:
      MemoryBase() { send_fn = std::bind(&MemoryBase::send, this, placeholders::_1); }
      virtual ~MemoryBase() {}
      virtual double clk_ns() = 0;
      virtual void tick() = 0;
      virtual bool send(Request req) = 0;
      virtual int pending_requests() = 0;
      virtual void finish(void) = 0;
      virtual void record_core(int coreid) = 0;
      virtual void reg_stats(uint num_cores) = 0;

      virtual void set_writeq_watermark(float hi_watermark, float lo_watermark) = 0;

      virtual void reload_options(const YAML::Node& config) {};
  };

  template <class T, template<class> class Controller = Controller >
  class Memory : public MemoryBase
  {  
    public:
      uint64_t max_address;

      std::vector<Controller<T>*> ctrls;
      T * spec;
      std::string mapping_file;
      bool use_mapping_file;
      bool dump_mapping;
      
      int tx_bits;

      Memory(const YAML::Node& config, std::vector<Controller<T>*> ctrls);
      ~Memory();

      double clk_ns();

      void record_core(int coreid) ;

      void tick();
      bool send(Request req);
      void finish(void);
      int pending_requests();

      void reg_stats(uint num_cores);

    public:
      void set_writeq_watermark(float hi_watermark, float lo_watermark);
      void reload_options(const YAML::Node& config);


    private:
      std::mt19937_64 allocator_rng;

    protected:
      ScalarStat dram_capacity;
      ScalarStat num_dram_cycles;
      ScalarStat num_incoming_requests;
      VectorStat num_read_requests;
      VectorStat num_write_requests;
      ScalarStat ramulator_active_cycles;
      VectorStat incoming_requests_per_channel;
      VectorStat incoming_read_reqs_per_channel;

      ScalarStat maximum_bandwidth;
      ScalarStat in_queue_req_num_sum;
      ScalarStat in_queue_read_req_num_sum;
      ScalarStat in_queue_write_req_num_sum;
      ScalarStat in_queue_req_num_avg;
      ScalarStat in_queue_read_req_num_avg;
      ScalarStat in_queue_write_req_num_avg;

    #ifndef INTEGRATED_WITH_GEM5
      VectorStat record_read_requests;
      VectorStat record_write_requests;
    #endif
  };



  MemoryBase* make_memory(const YAML::Node& config);
}

#include "Memory.tpp"

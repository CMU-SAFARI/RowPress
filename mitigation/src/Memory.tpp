#pragma once
#include "Memory.h"

namespace Ramulator
{
  template <class T, template<class> class Controller>
  Memory<T, Controller>::Memory(const YAML::Node& config, vector<Controller<T>*> ctrls):
  ctrls(ctrls),
  spec(ctrls[0]->channel->spec)
  {
    // make sure 2^N channels/ranks
    int* sz = spec->org_entry.count;
    assert((sz[0] & (sz[0] - 1)) == 0);
    assert((sz[1] & (sz[1] - 1)) == 0);
    // validate size of one transaction
    int tx = (spec->prefetch_size * spec->channel_width / 8);
    tx_bits = calc_log2(tx);
    assert((1<<tx_bits) == tx);

    // Calculate the total capacity of the memory system
    max_address = spec->channel_width / 8;
    for (int lv = 0; lv < int(T::Level::MAX); lv++) 
      max_address *= sz[lv];

    this->translation = make_translation(config, max_address);

    this->tick_unit = config["tick"].as<uint>(3);
  }

  template <class T, template<class> class Controller>
  Memory<T, Controller>::~Memory()
  {
    for (auto ctrl: ctrls)
      delete ctrl;
    delete spec;
  }

  template <class T, template<class> class Controller>
  double Memory<T, Controller>::clk_ns()
  {
    uint tCK_id = uint(T::TimingCons::tCK_ps);
    return spec->speed_entry[tCK_id];
  }

  template <class T, template<class> class Controller>
  void Memory<T, Controller>::record_core(int coreid) 
  {
  #ifndef INTEGRATED_WITH_GEM5
    record_read_requests[coreid] = num_read_requests[coreid];
    record_write_requests[coreid] = num_write_requests[coreid];
  #endif
    for (auto ctrl : ctrls)
      ctrl->record_core(coreid);
  }

  template <class T, template<class> class Controller>
  void Memory<T, Controller>::tick()
  {
    ++num_dram_cycles;
    int cur_que_req_num = 0;
    int cur_que_readreq_num = 0;
    int cur_que_writereq_num = 0;
    for (auto ctrl : ctrls) 
    {
      cur_que_req_num += ctrl->readq.size() + ctrl->writeq.size() + ctrl->pending.size();
      cur_que_readreq_num += ctrl->readq.size() + ctrl->pending.size();
      cur_que_writereq_num += ctrl->writeq.size();
    }

    in_queue_req_num_sum += cur_que_req_num;
    in_queue_read_req_num_sum += cur_que_readreq_num;
    in_queue_write_req_num_sum += cur_que_writereq_num;

    bool is_active = false;
    for (auto ctrl : ctrls) 
    {
      is_active = is_active || ctrl->is_active();
      ctrl->tick();
    }

    if (is_active)
      ramulator_active_cycles++;
  }

  template <class T, template<class> class Controller>
  bool Memory<T, Controller>::send(Request req)
  {
    uint64_t addr = req.addr;
    int coreid = req.coreid;

    // Each transaction size is 2^tx_bits, so first clear the lowest tx_bits bits
    clear_lower_bits(addr, tx_bits);

    // TODO: Integrate mapping file back to controller
    // if (use_mapping_file)
    //   apply_mapping(addr, req.addr_vec);
    // else
    ctrls[0]->addr_mapper->apply(addr, req);

    if(ctrls[req.addr_vec[0]]->enqueue(req)) 
    {
        // tally stats here to avoid double counting for requests that aren't enqueued
      ++num_incoming_requests;
      if (req.type == Request::Type::READ) 
      {
        ++num_read_requests[coreid];
        ++incoming_read_reqs_per_channel[req.addr_vec[int(T::Level::Channel)]];
      }
      if (req.type == Request::Type::WRITE) 
        ++num_write_requests[coreid];
      ++incoming_requests_per_channel[req.addr_vec[int(T::Level::Channel)]];
      return true;
    }

    return false;
  }

  template <class T, template<class> class Controller>
  int Memory<T, Controller>::pending_requests()
  {
    int reqs = 0;
    for (auto ctrl: ctrls)
      reqs += ctrl->readq.size() + ctrl->writeq.size() + ctrl->otherq.size() + ctrl->actq.size() + ctrl->pending.size();
    return reqs;
  }

  template <class T, template<class> class Controller>
  void Memory<T, Controller>::finish(void) 
  {
    dram_capacity = max_address;
    int* sz = spec->org_entry.count;
    maximum_bandwidth = spec->speed_entry[uint(T::TimingCons::rate)] * 1e6 * spec->channel_width * sz[int(T::Level::Channel)] / 8;
    uint64_t dram_cycles = num_dram_cycles.value();
    for (auto ctrl : ctrls) 
    {
      uint64_t read_req = uint64_t(incoming_read_reqs_per_channel[ctrl->channel->id].value());
      ctrl->finish(read_req, dram_cycles);
    }

    // finalize average queueing requests
    in_queue_req_num_avg = in_queue_req_num_sum.value() / dram_cycles;
    in_queue_read_req_num_avg = in_queue_read_req_num_sum.value() / dram_cycles;
    in_queue_write_req_num_avg = in_queue_write_req_num_sum.value() / dram_cycles;
  }


  template <class T, template<class> class Controller>
  void Memory<T, Controller>::set_writeq_watermark(float hi_watermark, float lo_watermark)
  {        
    if (hi_watermark < 0.0f || hi_watermark > 1.0f)
    {
      if (lo_watermark < 0.0f || lo_watermark > 1.0f)
        throw std::runtime_error(fmt::format("Invalid watermarks!"));
      else
        for (auto& ctrl : ctrls)
          ctrl->wr_low_watermark = lo_watermark;
    }
    else if (lo_watermark < 0.0f || lo_watermark > 1.0f)
    {
      if (hi_watermark < 0.0f || hi_watermark > 1.0f)
        throw std::runtime_error(fmt::format("Invalid watermarks!"));
      else
        for (auto& ctrl : ctrls)
          ctrl->wr_high_watermark = hi_watermark;
    }
    else
    {
      for (auto& ctrl : ctrls)
      {
        ctrl->wr_low_watermark = lo_watermark;
        ctrl->wr_high_watermark = hi_watermark;
      }
    }

    std::cout << fmt::format("Write queue watermark set: High = {}, Low = {}", ctrls[0]->wr_high_watermark, ctrls[0]->wr_low_watermark);
  }


  template <class T, template<class> class Controller>
  void Memory<T, Controller>::reload_options(const YAML::Node& config)
  {
    if (config["memory"])
      for (auto& ctrl: ctrls)
        ctrl->reload_options(config["memory"]);
  }


  template <class T, template<class> class Controller>
  void Memory<T, Controller>::reg_stats(uint num_cores)
  {
    for (auto ctrl: ctrls)
      ctrl->reg_stats(num_cores);

    int *sz = spec->org_entry.count;

    dram_capacity
        .name("dram_capacity")
        .desc("Number of bytes in simulated DRAM")
        .precision(0)
        ;
    dram_capacity = max_address;

    num_dram_cycles
        .name("dram_cycles")
        .desc("Number of DRAM cycles simulated")
        .precision(0)
        ;
    num_incoming_requests
        .name("incoming_requests")
        .desc("Number of incoming requests to DRAM")
        .precision(0)
        ;
    num_read_requests
        .init(num_cores)
        .name("read_requests")
        .desc("Number of incoming read requests to DRAM per core")
        .precision(0)
        ;
    num_write_requests
        .init(num_cores)
        .name("write_requests")
        .desc("Number of incoming write requests to DRAM per core")
        .precision(0)
        ;
    incoming_requests_per_channel
        .init(sz[int(T::Level::Channel)])
        .name("incoming_requests_per_channel")
        .desc("Number of incoming requests to each DRAM channel")
        ;
    incoming_read_reqs_per_channel
        .init(sz[int(T::Level::Channel)])
        .name("incoming_read_reqs_per_channel")
        .desc("Number of incoming read requests to each DRAM channel")
        ;

    ramulator_active_cycles
        .name("ramulator_active_cycles")
        .desc("The total number of cycles that the DRAM part is active (serving R/W)")
        .precision(0)
        ;
        
    maximum_bandwidth
        .name("maximum_bandwidth")
        .desc("The theoretical maximum bandwidth (Bps)")
        .precision(0)
        ;
    in_queue_req_num_sum
        .name("in_queue_req_num_sum")
        .desc("Sum of read/write queue length")
        .precision(0)
        ;
    in_queue_read_req_num_sum
        .name("in_queue_read_req_num_sum")
        .desc("Sum of read queue length")
        .precision(0)
        ;
    in_queue_write_req_num_sum
        .name("in_queue_write_req_num_sum")
        .desc("Sum of write queue length")
        .precision(0)
        ;
    in_queue_req_num_avg
        .name("in_queue_req_num_avg")
        .desc("Average of read/write queue length per memory cycle")
        .precision(6)
        ;
    in_queue_read_req_num_avg
        .name("in_queue_read_req_num_avg")
        .desc("Average of read queue length per memory cycle")
        .precision(6)
        ;
    in_queue_write_req_num_avg
        .name("in_queue_write_req_num_avg")
        .desc("Average of write queue length per memory cycle")
        .precision(6)
        ;
  #ifndef INTEGRATED_WITH_GEM5
    record_read_requests
        .init(num_cores)
        .name("record_read_requests")
        .desc("record read requests for this core when it reaches request limit or to the end")
        ;

    record_write_requests
        .init(num_cores)
        .name("record_write_requests")
        .desc("record write requests for this core when it reaches request limit or to the end")
        ;
  #endif
  }

};


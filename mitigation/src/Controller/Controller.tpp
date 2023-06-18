#pragma once

#include "Controller.h"

namespace Ramulator
{
  template <class T>
  Controller<T>* make_controller(const YAML::Node& config, DRAM<T>* channel)
  {
    if (!config["controller"])
      throw std::runtime_error("Memory controller configuration (controller:) not found!");

    const YAML::Node& controller_config = config["controller"];
    std::string controller_type = controller_config["type"].as<std::string>("");

    Controller<T>* controller = nullptr;

    if      (controller_type == "")
      controller = new Controller<T>(controller_config, channel);
    else
      throw std::runtime_error(fmt::format("Unrecognized controller type: {}!", controller_type));

    return controller;
  }

  template <class T, template<class> class Controller = Controller >
  std::vector<Controller<T>*> make_DRAM_and_controllers(const YAML::Node& config)
  {
    T* spec = new T(config);

    // According to the spec, build individual DRAM channels and respective controllers
    int C = spec->org_entry.count[uint(T::Level::Channel)];
    std::vector<Controller<T>*> ctrls;
    for (int c = 0 ; c < C ; c++) {
      DRAM<T>* channel = new DRAM<T>(spec, T::Level::Channel);
      channel->id = c;
      channel->reg_stats("");
      Controller<T>* ctrl = make_controller(config, channel);
      ctrls.push_back(ctrl);
    }

    return ctrls;
  }


  template <typename T>
  Controller<T>::Controller(const YAML::Node& config, DRAM<T>* channel) :
  channel(channel),
  cmd_trace_files(channel->children.size())
  {
    addr_mapper = make_addr_mapper<T> (config, this);
    scheduler   = make_scheduler<T> (config, this);
    rowpolicy   = make_rowpolicy<T> (config, this);
    rowtable    = make_rowtable<T> (config, this);
    refresh     = make_refresh<T>  (config, this);
    rh_defense  = make_refresh_based_defense<T> (config, this);

    record_cmd_trace = config["record_cmd_trace"].as<bool>(false);
    print_cmd_trace = config["print_cmd_trace"].as<bool>(false);
    track_activation_count = config["track_activation_count"].as<bool>(false);

    std::string row_policy_type = config["row_policy"]["type"].as<std::string>("");
    is_dumb_closed_page_policy = row_policy_type == "ClosedDumb"; 


    // Assuming channel->spec is already frozen at this point?
    if constexpr (Traits::has_BankGroup<T>)
    {
      no_rows_per_rank =  channel->spec->org_entry.count[int(T::Level::Row)] * 
                          channel->spec->org_entry.count[int(T::Level::Bank)] * 
                          channel->spec->org_entry.count[int(T::Level::BankGroup)];
      no_rows_per_bg = channel->spec->org_entry.count[int(T::Level::Row)] * 
                       channel->spec->org_entry.count[int(T::Level::Bank)];
      no_rows_per_bank = channel->spec->org_entry.count[int(T::Level::Row)];                        
    }
    else
    {
      no_rows_per_rank =  channel->spec->org_entry.count[int(T::Level::Row)] * 
                          channel->spec->org_entry.count[int(T::Level::Bank)];                          
      no_rows_per_bank = channel->spec->org_entry.count[int(T::Level::Row)];
      no_rows_per_bg = 0;
    }

    activation_count_dump_file = config["activation_count_dump_file"].as<std::string>("dump.txt");

    if (track_activation_count)
    {
      int no_ranks = channel->spec->org_entry.count[int(T::Level::Rank)];
      activation_count = std::vector<int>(no_rows_per_rank * no_ranks, 0);
    }

    if (record_cmd_trace)
    {
      if (config["cmd_trace_prefix"])
        cmd_trace_prefix = config["cmd_trace_prefix"].as<std::string>("");

      std::string prefix = cmd_trace_prefix + "chan-" + to_string(channel->id) + "-rank-";
      std::string suffix = ".cmdtrace";
      for (unsigned int i = 0; i < channel->children.size(); i++)
        cmd_trace_files[i].open(prefix + to_string(i) + suffix);
    }

    close_after_nticks = config["close_after_nticks"].as<int>(0);

    otherq.max = config["otherq_max"].as<int>(1024);
  }

  template <typename T>
  Controller<T>::~Controller()
  {
    delete scheduler;
    delete rowpolicy;
    delete rowtable;
    delete channel;
    delete refresh;
    for (auto& file : cmd_trace_files)
        file.close();
    cmd_trace_files.clear();
  }


  // TODO: Workaround for one bug in the current implementation
  // must be fixed properly eventually
  // TODO: fix this
  template <typename T>
  void Controller<T>::reload_options(const YAML::Node& config)
  {
    if (!track_activation_count)
    {
      track_activation_count = config["controller"]["track_activation_count"].as<bool>(false);    
      
      std::cout << "track_activation_count: " << track_activation_count << std::endl;
      
      if (track_activation_count)
      {
        int no_ranks = channel->spec->org_entry.count[int(T::Level::Rank)];
        activation_count = std::vector<int>(no_rows_per_rank * no_ranks, 0);
      }
    }

    if (rh_defense == nullptr)
      rh_defense  = make_refresh_based_defense<T> (config["controller"], this);

  }

  template <typename T>
  void Controller<T>::finish(long read_req, long dram_cycles)
  {
    read_latency_avg = read_latency_sum.value() / read_req;
    req_queue_length_avg = req_queue_length_sum.value() / dram_cycles;
    read_req_queue_length_avg = read_req_queue_length_sum.value() / dram_cycles;
    write_req_queue_length_avg = write_req_queue_length_sum.value() / dram_cycles;
    row_open_cycles_avg = row_open_cycles_sum.value() / activate_commands.value();

    if (track_activation_count)
      dump_activation_count();
    // call finish function of each channel
    channel->finish(dram_cycles, clk);
  }

  template <typename T>
  ReqQueue& Controller<T>::get_queue(Request& req)
  {
    Request::Type type = req.type;
    switch (int(type))
    {
      case int(Request::Type::READ): return readq;
      case int(Request::Type::WRITE): return writeq;
      default: return otherq;
    }
  }

  template <typename T>
  bool Controller<T>::enqueue(Request& req)
  {
    ReqQueue& queue = get_queue(req);
    if (queue.max == queue.size())
      return false;

    req.arrive = clk;
    queue.q.push_back(req);
    // shortcut for read requests, if a write to same addr exists
    // necessary for coherence
    if (req.type == Request::Type::READ && find_if(writeq.q.begin(), writeq.q.end(),
          [req](Request& wreq){ return req.addr == wreq.addr;}) != writeq.q.end())
    {
      req.depart = clk + 1;
      pending.push_back(req);
      readq.q.pop_back();
    }
    return true;
  }

  template <typename T>
  void Controller<T>::serve_completed_reads()
  {
    if (pending.size()) 
    {
      Request& req = pending[0];
      // depart and arrive = -1 encode some meaningful information
      // but we are comparing depart (long) here with clk (unsigned long)
      // which may end up resulting in an unexpected outcome
      // TODO: This is a bug, we need to fix it
      assert((clk <= (1ULL << (sizeof(long int)*8 - 1)) - 1) && "Clock made too much progress");
      if (req.depart <= (long int) clk) 
      {
        if (req.depart - req.arrive > 1) 
        { // this request really accessed a row
          read_latency_sum += req.depart - req.arrive;
          channel->update_serving_requests(req.addr_vec.data(), -1, clk);
        }
        req.callback(req);
        pending.pop_front();
      }
    }
  }

  template <typename T>
  void Controller<T>::set_write_mode()
  {
    if (!write_mode)
    {
      // yes -- write queue is almost full or read queue is empty
      if (writeq.size() > uint(wr_high_watermark * writeq.max) || readq.size() == 0)
        write_mode = true;
    }
    else 
    {
      // no -- write queue is almost empty and read queue is not empty
      if (writeq.size() < uint(wr_low_watermark * writeq.max) && readq.size() != 0)
        write_mode = false;
    }
  }

  template <typename T>
  bool Controller<T>::find_request_to_schedule(ReqQueue*& queue, std::list<Request>::iterator& req, typename T::Command& cmd, std::vector<int>& addr_vec)
  {
    ReqQueue* _queue = nullptr;
    typename T::Command _cmd = T::Command::MAX;

    // 1. check the high-priority actq
    _queue = &actq;
    auto _req = scheduler->get_head(_queue->q);
    bool is_req_valid = (_req != _queue->q.end());
    if(is_req_valid) 
    {
      _cmd = get_next_cmd(_req);
      is_req_valid = is_ready(_cmd, _req->addr_vec);
    }

    // 1.2: Check if there is a row to close
    if (!is_req_valid && (close_after_nticks > 0))
    {
      // Check the rowtable to see row open times
      // If a row has been open for more than close_after_nticks
      // then close it
      // TODO hardcoded
      for (int r = 0 ; r < 2 ; r++)
      {
        for (int bg = 0 ; bg < 4 ; bg++)
        {
          for (int b = 0 ; b < 4 ; b++)
          {
            std::vector<int> __addr_vec = {0, r, bg, b, 0, 0};
            int openrow = rowtable->get_open_row(__addr_vec);
            if (openrow != -1)
            {
              __addr_vec[4] = openrow;
              int opencycles = rowtable->get_open_time(__addr_vec, clk);

              if (opencycles > close_after_nticks)
              {
                queue = nullptr;
                cmd = T::Command::PRE;
                addr_vec = __addr_vec;
                std::vector<int> rg(__addr_vec.begin(), __addr_vec.begin() + int(T::Level::Row));

                // if there is a request that targets the same
                // rowgroup and is not served yet, do not close
                for (auto it = actq.q.begin(); it != actq.q.end(); it++)
                {
                  std::vector<int> it_rowgroup(it->addr_vec.begin(), it->addr_vec.begin() + int(T::Level::Row));
                  if (it_rowgroup == rg)
                    return false;
                }

                //printf("Precharging row %d after %d cycles\n", openrow, opencycles);
                return true;
              }
            }
          }
        }
      }
    }

    // 2. If no requests ready in the actq, check the other queues
    if (!is_req_valid) 
    {
      _queue = !write_mode ? &readq : &writeq;
      if (otherq.size())
      // "other" requests have higher priority than reads/writes
        _queue = &otherq;  

      _req = scheduler->get_head(_queue->q);
      is_req_valid = (_req != _queue->q.end());

      if(is_req_valid)
      {
        _cmd = get_next_cmd(_req);
        is_req_valid = is_ready(_cmd, _req->addr_vec);

        // Prevent this request from precharging
        // already opened but not accessed yet rows
        if (is_req_valid && channel->spec->is_closing(_cmd))
        {
          auto begin = _req->addr_vec.begin();
          auto end = begin + int(T::Level::Row);
          std::vector<int> rowgroup(begin, end);

          // if there is a request that targets the same
          // rowgroup and is not served yet, do not close
          for (auto it = actq.q.begin(); it != actq.q.end(); it++)
          {
            std::vector<int> it_rowgroup(it->addr_vec.begin(), it->addr_vec.begin() + int(T::Level::Row));
            if (it_rowgroup == rowgroup)
            {
              is_req_valid = false;
              break;
            }
          }
        }
      }
    }

    // 3. We could not find a request to schedule
    if (!is_req_valid) 
    {
      // Speculatively issue a PRE according to the rowpolicy
      cmd = T::Command::PRE;
      addr_vec = rowpolicy->get_victim(cmd);

      if (addr_vec.empty())
        return false;
      else 
        return true;
    }
    else
    {
      queue = _queue;
      req = _req;
      cmd = _cmd;
      addr_vec = _req->addr_vec;
      return true;
    }
  }


  template <typename T>
  void Controller<T>::serve_pending_AP()
  {
    for(auto addr_vec_itr = pending_autoprecharges.begin(); addr_vec_itr != pending_autoprecharges.end(); )
    {
      if(is_ready(T::Command::PRE, *addr_vec_itr))
      {
        issue_cmd(T::Command::PRE, *addr_vec_itr);
        addr_vec_itr = pending_autoprecharges.erase(addr_vec_itr);
      }
      else
        addr_vec_itr++;
    }
  }

  template <typename T>
  void Controller<T>::process_activate(const std::vector<int>& addr_vec)
  {
    int row_index;
    if constexpr (Traits::has_BankGroup<T>)
    {
      row_index = addr_vec[int(T::Level::Rank)] * no_rows_per_rank + 
                  addr_vec[int(T::Level::BankGroup)] * no_rows_per_bg +
                  addr_vec[int(T::Level::Bank)] * no_rows_per_bank + 
                  addr_vec[int(T::Level::Row)];
    }
    else
    {
      row_index = addr_vec[int(T::Level::Rank)] * no_rows_per_rank + 
                  addr_vec[int(T::Level::Bank)] * no_rows_per_bank + 
                  addr_vec[int(T::Level::Row)];
    }
    activation_count[row_index]++;
  }

  template <typename T>
  void Controller<T>::dump_activation_count()
  {
    std::ofstream out(activation_count_dump_file, std::ios::out);
    if (!out.is_open())
      throw std::runtime_error("Could not open activation_count_dump_file for writing");

    if constexpr (Traits::has_BankGroup<T>)
    {
      for(int r = 0 ; r < channel->spec->org_entry.count[int(T::Level::Rank)] ; r++)
        for (int bg = 0 ; bg < channel->spec->org_entry.count[int(T::Level::BankGroup)] ; bg++)
          for (int b = 0 ; b < channel->spec->org_entry.count[int(T::Level::Bank)] ; b++)
            for (int rw = 0 ; rw < channel->spec->org_entry.count[int(T::Level::Row)] ; rw++)
              if (activation_count[r * no_rows_per_rank + bg * no_rows_per_bg + b * no_rows_per_bank + rw] != 0)
                out << r << " " << bg << " " << b << " " << rw << " - " << 
                  activation_count[r * no_rows_per_rank + bg * no_rows_per_bg + b * no_rows_per_bank + rw] << std::endl;
    }
    else
    {
      for(int r = 0 ; r < channel->spec->org_entry.count[int(T::Level::Rank)] ; r++)
          for (int b = 0 ; b < channel->spec->org_entry.count[int(T::Level::Bank)] ; b++)
            for (int rw = 0 ; rw < channel->spec->org_entry.count[int(T::Level::Row)] ; rw++)
              if (activation_count[r * no_rows_per_rank + b * no_rows_per_bank + rw] != 0)
                out << r << " " << b << " " << rw << " - " << 
                  activation_count[r * no_rows_per_rank + b * no_rows_per_bank + rw] << std::endl;
    }
    out.close();
  }

  template <typename T>
  void Controller<T>::tick()
  {
    clk++;

    req_queue_length_sum += readq.size() + writeq.size() + pending.size();
    read_req_queue_length_sum += readq.size() + pending.size();
    write_req_queue_length_sum += writeq.size();

    /*** 1. Serve completed reads ***/
    serve_completed_reads();

    /*** 2. Refresh scheduler ***/
    refresh->tick();

    /*** 3. Tick RowHammer defense ***/
    if (rh_defense)
      rh_defense->tick();

    /*** 4. Should we schedule writes? ***/
    set_write_mode();

    /*** 5. Find the best command to schedule***/
    ReqQueue* queue = nullptr;
    std::list<Request>::iterator req;
    typename T::Command cmd;
    std::vector<int> addr_vec;

    bool found_cmd = find_request_to_schedule(queue, req, cmd, addr_vec);
    if (found_cmd)
    {
      // Update statistics that is related to the entire request
      // only if we actually scheduled a request, i.e., not speculative precharge
      if (queue != nullptr)
      {
        if (!req->stats_updated)
        {
          req->stats_updated = true;
          update_request_stats(req);
        }
      }

      issue_cmd(cmd, addr_vec);

      // Are we serving a real request or just speculatively precharging?
      if (queue != nullptr)
      {
        // Check whether this is the last command (which finishes the request)
        if (cmd == channel->spec->translate[int(req->type)]) 
        {
          if (req->type == Request::Type::READ) 
          // Set a future completion time for read requests
          {
            req->depart = clk + channel->spec->read_latency;
            pending.push_back(*req);
          }
          else if (req->type == Request::Type::WRITE) 
          {
            channel->update_serving_requests(req->addr_vec.data(), -1, clk);
            // req->callback(*req);
          }
          queue->q.erase(req);
        }
        else
        {
          if(channel->spec->is_opening(cmd)) 
          // Promote the request that causes activation to the actq
          {
            actq.q.push_back(*req);
            queue->q.erase(req);
          }
        }
      }
    }

    /*** 5. Implicitly serve any pending PRE generated by RDA/WRA commands ***/
    serve_pending_AP();
    

  }

  template <typename T>
  bool Controller<T>::is_ready(list<Request>::iterator req)
  {
      typename T::Command cmd = get_next_cmd(req);
      return channel->check(cmd, req->addr_vec.data(), clk);
  }

  template <typename T>
  bool Controller<T>::is_ready(typename T::Command cmd, const vector<int>& addr_vec)
  {
      return channel->check(cmd, addr_vec.data(), clk);
  }

  template <typename T>
  bool Controller<T>::is_row_hit(list<Request>::iterator req)
  {
      // cmd must be decided by the request type, not the first cmd
      typename T::Command cmd = channel->spec->translate[int(req->type)];
      return channel->check_row_hit(cmd, req->addr_vec.data());
  }

  template <typename T>
  bool Controller<T>::is_row_hit(typename T::Command cmd, const vector<int>& addr_vec)
  {
      return channel->check_row_hit(cmd, addr_vec.data());
  }

  template <typename T>
  bool Controller<T>::is_row_open(list<Request>::iterator req)
  {
      // cmd must be decided by the request type, not the first cmd
      typename T::Command cmd = channel->spec->translate[int(req->type)];
      return channel->check_node_open(cmd, req->addr_vec.data());
  }

  template <typename T>
  bool Controller<T>::is_row_open(typename T::Command cmd, const vector<int>& addr_vec)
  {
      return channel->check_node_open(cmd, addr_vec.data());
  }

  // For telling whether this channel is busying in processing read or write
  template <typename T>
  bool Controller<T>::is_active() {
    return (channel->cur_serving_requests > 0);
  }

  // For telling whether this channel is under refresh
  template <typename T>
  bool Controller<T>::is_refresh() {
    return clk <= channel->end_of_refreshing;
  }


  template <typename T>
  void Controller<T>::record_core(int coreid) {
#ifndef INTEGRATED_WITH_GEM5
    record_read_hits[coreid] = read_row_hits[coreid];
    record_read_misses[coreid] = read_row_misses[coreid];
    record_read_conflicts[coreid] = read_row_conflicts[coreid];
    record_write_hits[coreid] = write_row_hits[coreid];
    record_write_misses[coreid] = write_row_misses[coreid];
    record_write_conflicts[coreid] = write_row_conflicts[coreid];
#endif
  }

  template <typename T>
  typename T::Command Controller<T>::get_next_cmd(list<Request>::iterator req)
  {
      typename T::Command cmd = channel->spec->translate[int(req->type)];
      return channel->decode(cmd, req->addr_vec.data());
  }

  // upgrade to an autoprecharge command by checking if it is the last request to the opened row
  template <typename T>
  void Controller<T>::cmd_upgrade_AP(typename T::Command& cmd, const vector<int>& addr_vec) 
  {
    ReqQueue* queue = write_mode ? &writeq : &readq;

    auto begin = addr_vec.begin();
    std::vector<int> rowgroup(begin, begin + int(T::Level::Row) + 1);

    int num_row_hits = 0;

    for (auto itr = queue->q.begin(); itr != queue->q.end(); ++itr) 
    {
      if (is_row_hit(itr)) 
      {
        auto begin2 = itr->addr_vec.begin();
        std::vector<int> rowgroup2(begin2, begin2 + int(T::Level::Row) + 1);
        if(rowgroup == rowgroup2)
          num_row_hits++;
      }
    }

    if(num_row_hits == 0) 
    {
      ReqQueue* queue = &actq;
      for (auto itr = queue->q.begin(); itr != queue->q.end(); ++itr) 
      {
        if (is_row_hit(itr)) 
        {
          auto begin2 = itr->addr_vec.begin();
          vector<int> rowgroup2(begin2, begin2 + int(T::Level::Row) + 1);
          if(rowgroup == rowgroup2)
            num_row_hits++;
        }
      }
    }

    assert(num_row_hits > 0); // The current request should be a hit,
                              // so there should be at least one request
                              // that hits in the current open row

    if (is_dumb_closed_page_policy)
      num_row_hits = 1;

    if(num_row_hits == 1) 
    {
      if(cmd == T::Command::RD)
        cmd = T::Command::RDA;
      else if (cmd == T::Command::WR)
        cmd = T::Command::WRA;
      else
        assert(false && "Unimplemented command type.");

      pending_autoprecharges.push_back(addr_vec);
    }
  }

  template <typename T>
  void Controller<T>::issue_cmd(typename T::Command cmd, const vector<int>& addr_vec)
  {
    if constexpr (Traits::has_RH_defense<T>)
      refresh->update_refresh_scheduler(cmd, addr_vec);

    if(channel->spec->is_accessing(cmd) && dynamic_cast<ClosedAP<T>*>(rowpolicy) != nullptr)
    //
      cmd_upgrade_AP(cmd, addr_vec);

    if(channel->spec->is_accessing(cmd) && dynamic_cast<ClosedDumb<T>*>(rowpolicy) != nullptr)
    //
      cmd_upgrade_AP(cmd, addr_vec);

    assert(is_ready(cmd, addr_vec));
    channel->update(cmd, addr_vec.data(), clk);

    if(cmd == T::Command::PRE)
    {
      if(rowtable->get_hits(addr_vec, true) == 0)
        useless_activates++;
      row_open_cycles_sum += rowtable->get_open_time(addr_vec, clk);
      activate_commands++;
    }


    if (rh_defense != nullptr)
    {
      int open_row = rowtable->get_open_row(addr_vec);
      std::vector<int> precharged_address_vec = addr_vec;
      precharged_address_vec[int(T::Level::Row)] = open_row;
      uint64_t open_for_nclocks = rowtable->get_open_time(precharged_address_vec, clk);
      rh_defense->update(cmd, precharged_address_vec, open_for_nclocks);
    }

    if (track_activation_count && cmd == T::Command::ACT)
      process_activate(addr_vec);

    rowtable->update(cmd, addr_vec, clk);

    if (record_cmd_trace)
      dump_trace(cmd, addr_vec);

    if (print_cmd_trace)
      print_trace(cmd, addr_vec);
  }


  template <typename T>
  void Controller<T>::dump_trace(typename T::Command cmd, const vector<int>& addr_vec)
  {
    auto& file = cmd_trace_files[addr_vec[int(T::Level::Rank)]];
    const std::string& cmd_name = channel->spec->command_str[int(cmd)];
    file << fmt::format("{},{}", clk, cmd_name);

    if (channel->spec->scope[int(cmd)] == T::Level::Rank)
      file << "\n";
    else
    {
      int bank_id = addr_vec[int(T::Level::Bank)];
      if constexpr (Traits::has_BankGroup<T>)
        bank_id += addr_vec[int(T::Level::Bank) - 1] * channel->spec->org_entry.count[int(T::Level::Bank)];

      file << fmt::format(",{}\n", bank_id);
    }
  };

  template <typename T>
  void Controller<T>::print_trace(typename T::Command cmd, const vector<int>& addr_vec)
  {
    printf("%5s %10ld:", channel->spec->command_str[int(cmd)].c_str(), clk);
    for (int lev = 0; lev < int(T::Level::MAX); lev++)
    {
      // Do not print levels lower than Level::Bank (e.g., row, column)
      // actually prints -1 to keep the printout consistent
      if ((int(cmd) == int(T::Command::PRE)) && lev > int(T::Level::Bank))
        printf(" %5d", -1);
      else
        printf(" %5d", addr_vec[lev]);
    }
    printf("\n");
  };

  template <typename T>
  void Controller<T>::reg_stats(uint num_cores)
  {
    row_hits
        .name("row_hits_channel_"+to_string(channel->id) + "_core")
        .desc("Number of row hits per channel per core")
        .precision(0)
        ;
    row_misses
        .name("row_misses_channel_"+to_string(channel->id) + "_core")
        .desc("Number of row misses per channel per core")
        .precision(0)
        ;
    row_conflicts
        .name("row_conflicts_channel_"+to_string(channel->id) + "_core")
        .desc("Number of row conflicts per channel per core")
        .precision(0)
        ;
    preventive_refreshes
        .name("preventive_refreshes_channel_"+to_string(channel->id) + "_core")
        .desc("Number of preventive refreshes per channel per core")
        .precision(0)
        ;    
    
    row_open_cycles_sum
        .name("row_open_cycles_sum_"+to_string(channel->id) + "_core")
        .desc("How long all rows remained active per channel per core")
        .precision(0)
        ; 
    row_open_cycles_avg
        .name("row_open_cycles_avg_"+to_string(channel->id) + "_core")
        .desc("How long each row remained active on average per channel per core")
        .precision(6)
        ; 
    activate_commands
        .name("activate_commands_"+to_string(channel->id) + "_core")
        .desc("How many activate commands issued per channel")
        .precision(0)
        ; 

    read_row_hits
        .init(num_cores)
        .name("read_row_hits_channel_"+to_string(channel->id) + "_core")
        .desc("Number of row hits for read requests per channel per core")
        .precision(0)
        ;
    read_row_misses
        .init(num_cores)
        .name("read_row_misses_channel_"+to_string(channel->id) + "_core")
        .desc("Number of row misses for read requests per channel per core")
        .precision(0)
        ;
    read_row_conflicts
        .init(num_cores)
        .name("read_row_conflicts_channel_"+to_string(channel->id) + "_core")
        .desc("Number of row conflicts for read requests per channel per core")
        .precision(0)
        ;

    write_row_hits
        .init(num_cores)
        .name("write_row_hits_channel_"+to_string(channel->id) + "_core")
        .desc("Number of row hits for write requests per channel per core")
        .precision(0)
        ;
    write_row_misses
        .init(num_cores)
        .name("write_row_misses_channel_"+to_string(channel->id) + "_core")
        .desc("Number of row misses for write requests per channel per core")
        .precision(0)
        ;
    write_row_conflicts
        .init(num_cores)
        .name("write_row_conflicts_channel_"+to_string(channel->id) + "_core")
        .desc("Number of row conflicts for write requests per channel per core")
        .precision(0)
        ;

    useless_activates
        .name("useless_activates_"+to_string(channel->id)+ "_core")
        .desc("Number of useless activations. E.g, ACT -> PRE w/o RD or WR")
        .precision(0)
        ;

    read_transaction_bytes
        .name("read_transaction_bytes_"+to_string(channel->id))
        .desc("The total byte of read transaction per channel")
        .precision(0)
        ;
    write_transaction_bytes
        .name("write_transaction_bytes_"+to_string(channel->id))
        .desc("The total byte of write transaction per channel")
        .precision(0)
        ;

    read_latency_sum
        .name("read_latency_sum_"+to_string(channel->id))
        .desc("The memory latency cycles (in memory time domain) sum for all read requests in this channel")
        .precision(0)
        ;
    read_latency_avg
        .name("read_latency_avg_"+to_string(channel->id))
        .desc("The average memory latency cycles (in memory time domain) per request for all read requests in this channel")
        .precision(6)
        ;

    req_queue_length_sum
        .name("req_queue_length_sum_"+to_string(channel->id))
        .desc("Sum of read and write queue length per memory cycle per channel.")
        .precision(0)
        ;
    req_queue_length_avg
        .name("req_queue_length_avg_"+to_string(channel->id))
        .desc("Average of read and write queue length per memory cycle per channel.")
        .precision(6)
        ;

    read_req_queue_length_sum
        .name("read_req_queue_length_sum_"+to_string(channel->id))
        .desc("Read queue length sum per memory cycle per channel.")
        .precision(0)
        ;
    read_req_queue_length_avg
        .name("read_req_queue_length_avg_"+to_string(channel->id))
        .desc("Read queue length average per memory cycle per channel.")
        .precision(6)
        ;

    write_req_queue_length_sum
        .name("write_req_queue_length_sum_"+to_string(channel->id))
        .desc("Write queue length sum per memory cycle per channel.")
        .precision(0)
        ;
    write_req_queue_length_avg
        .name("write_req_queue_length_avg_"+to_string(channel->id))
        .desc("Write queue length average per memory cycle per channel.")
        .precision(6)
        ;

  #ifndef INTEGRATED_WITH_GEM5
    record_read_hits
        .init(num_cores)
        .name("record_read_hits")
        .desc("record read hit count for this core when it reaches request limit or to the end")
        ;

    record_read_misses
        .init(num_cores)
        .name("record_read_misses")
        .desc("record_read_miss count for this core when it reaches request limit or to the end")
        ;

    record_read_conflicts
        .init(num_cores)
        .name("record_read_conflicts")
        .desc("record read conflict count for this core when it reaches request limit or to the end")
        ;

    record_write_hits
        .init(num_cores)
        .name("record_write_hits")
        .desc("record write hit count for this core when it reaches request limit or to the end")
        ;

    record_write_misses
        .init(num_cores)
        .name("record_write_misses")
        .desc("record write miss count for this core when it reaches request limit or to the end")
        ;

    record_write_conflicts
        .init(num_cores)
        .name("record_write_conflicts")
        .desc("record write conflict for this core when it reaches request limit or to the end")
        ;
  #endif
  }

  template <typename T>
  void Controller<T>::update_request_stats(std::list<Request>::iterator& req)
  {
    int coreid = req->coreid;

    if (req->type == Request::Type::PREVENTIVE_REFRESH)
      ++preventive_refreshes;

    if (req->type == Request::Type::READ || req->type == Request::Type::WRITE)
      channel->update_serving_requests(req->addr_vec.data(), 1, clk);

    int tx = (channel->spec->prefetch_size * channel->spec->channel_width / 8);

    if (req->type == Request::Type::READ) 
    {
      read_transaction_bytes += tx;

      if (is_row_hit(req)) 
      {
        ++read_row_hits[coreid];
        ++row_hits;
      } 
      else if (is_row_open(req)) 
      {
        ++read_row_conflicts[coreid];
        ++row_conflicts;
      } 
      else 
      {
        ++read_row_misses[coreid];
        ++row_misses;
      }
    } 
    else if (req->type == Request::Type::WRITE) 
    {
      write_transaction_bytes += tx;

      if (is_row_hit(req)) 
      {
        ++write_row_hits[coreid];
        ++row_hits;
      } 
      else if (is_row_open(req)) 
      {
        ++write_row_conflicts[coreid];
        ++row_conflicts;
      } 
      else 
      {
        ++write_row_misses[coreid];
        ++row_misses;
      }
    }
  }

} /* namespace Ramulator */

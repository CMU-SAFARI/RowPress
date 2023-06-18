#pragma once

#include <cassert>
#include <cstdio>
#include <deque>
#include <fstream>
#include <list>
#include <string>
#include <vector>

#include "bitmanip.h"
#include "Config.h"
#include "DRAM.h"
#include "Request.h"
#include "AddrMapper.h"
#include "Refresh.h"
#include "Scheduler.h"
#include "RowPolicy.h"
#include "RowTable.h"
#include "Scheduler.h"
#include "Statistics.h"
#include "RefreshBasedDefense.h"

namespace Ramulator
{
  class ReqQueue 
  {
    public:
      std::list<Request> q;
      size_t max = 32;
    
    public:
      size_t size() { return q.size(); }
  };

  template <class T>
  class RefreshBase;

  template <class T>
  RefreshBase<T>* make_refresh(const YAML::Node& config, Controller<T>* ctrl);

  template <class T>
  class RefreshBasedDefense;

  template <class T>
  RefreshBasedDefense<T>* make_refresh_based_defense(const YAML::Node& config, Controller<T>* ctrl);


  template <typename T>
  class Controller
  {
    // Basic data for a memory controller
    public:
      uint64_t clk = 0;
      DRAM<T>* channel;

      AddrMapperBase<T>* addr_mapper;
      SchedulerBase<T>* scheduler;          ///< Determines which request's commands get issued.
      RefreshBase<T>* refresh;              ///< Tracks refresh progress and inject REF commands.
      RefreshBasedDefense<T>* rh_defense;   ///< Refresh based RowHammer defense mechanism.
      RowPolicyBase<T>* rowpolicy;          ///< Determines the row-policy (e.g., closed-row vs. open-row).
      RowTable<T>* rowtable;            ///< Tracks metadata about rows (e.g., which are open and for how long).

      ReqQueue readq;                   ///< The queue for read requests.
      ReqQueue writeq;                  ///< The queue for write requests.
      ReqQueue actq;                    ///< Read and write requests for which ACT was issued are moved here.
      ReqQueue otherq;                  ///< The queue for all "other" requests (e.g., refresh).

      std::deque<Request> pending;      ///< Read requests that are about to receive data from DRAM
      
      std::vector<std::vector<int>> pending_autoprecharges; ///< Stores pending autoprecharges

      bool write_mode = false;          ///< Drain the write queue: Write requests are acculumated before being serviced.
      float wr_high_watermark = 0.8f;   ///< Threshold for switching to write mode.
      float wr_low_watermark = 0.2f;    ///< Threshold for switching back to read mode.
      
      // count the number of activations issued to every row
      bool track_activation_count = false;
      std::string activation_count_dump_file;
      std::vector<int> activation_count;
      int no_rows_per_rank;
      int no_rows_per_bank;
      int no_rows_per_bg;

      bool is_dumb_closed_page_policy;

      int close_after_nticks = 0;

    // Parameters for dumping/printing command traces (for debugging purposes)
    public:
      bool record_cmd_trace = false;
      bool print_cmd_trace = false;

      std::string cmd_trace_prefix = "cmd-trace-";
      std::vector<std::ofstream> cmd_trace_files;


    public:
      /**
       * @brief    Construct a new Controller object
       * 
       * @param    config         The YAML node containing the controller configuration.
       * @param    channel        The pointer to the DRAM hierarchy managed by the controller.
       */
      Controller(const YAML::Node& config, DRAM<T>* channel);
      virtual ~Controller();

      /**
       * @brief    An interface to reload controller options after warmup.
       * 
       * @param    config         A YAML node containing options that are applied after warmup.
       */
      virtual void reload_options(const YAML::Node& config);

    // Key interface methods to the memory subsystem
    public:
      /**
       * @brief    Ticks the memory controller.
       * 
       * In each tick, the memory controller performs the following steps:
       * 1. Checks the pending queue for completed reads. If a request in the pending queue has satisfied 
       *    its read latency (nCL + nBL), we call the callback to notify the processor and remove it from
       *    the pending queue.
       * 2. Ticks the refresh scheduler.
       * 3. Checks whether to drain the write queue (or switch back to read mode) depending on the watermark
       *    and the depth of the writeq.
       * 4. Consult the scheduler to find a best request whose corresponding command can be scheduled.
       *    Here, we first check actq (requests that have been issued the ACT commands are moved here)
       *    because we do not want to waste activations. Then depending on the write mode, we check  
       *    readq/writeq, followed by otherq (REF commands are here). If we can find such a request
       *    (i.e., checking the timing constraint of the corresponding command is satisfied), then the
       *    corresponding command is issued.
       * 5. If the issued command activates a row for a request, we move the request from its original
       *    queue to actq and returns.
       * 6. If the issued command is a terminal command of a request, we set the corresponding read 
       *    latency (nCL + nBL) and move it to the pending queue.
       * 
       */
      virtual void tick();

      /**
       * @brief    Sends a request to the corresponsing request queue in the memory controller.
       * 
       * @param    req            The request.
       * @return   true           Return true if successful.
       * @return   false          Return false if the corresponding request queue is full.
       */
      virtual bool enqueue(Request& req);

      /**
       * @brief    Get a reference to the corresponding queue of the request.
       * 
       * @param    req            The request.
       * @return   ReqQueue&      A reference to the corresponding queue of the request.
       */
      virtual ReqQueue& get_queue(Request& req);

    // Interfaces to query the states of the memory controller (queries the DRAM state machine hierarchically).
    public:
      /**
       * @brief    Checks whether a command is ready (i.e., timing constraints satisfied).
       * 
       * @param    cmd            The command.
       * @param    addr_vec       The address associated with the command.
       *      
       */
      bool is_ready(typename T::Command cmd, const vector<int>& addr_vec);
      bool is_ready(list<Request>::iterator req);

      /**
       * @brief    Checks whether a command causes a row-hit.
       * 
       * @param    cmd            The command.
       * @param    addr_vec       The address associated with the command.
       *       
       */
      bool is_row_hit(typename T::Command cmd, const vector<int>& addr_vec);
      bool is_row_hit(list<Request>::iterator req);

      /**
       * @brief    Checks whether a row is open for a command.
       * 
       * @param    cmd            The command.
       * @param    addr_vec       The address associated with the command.
       *      
       */
      bool is_row_open(typename T::Command cmd, const vector<int>& addr_vec);
      bool is_row_open(list<Request>::iterator req);

      /**
       * @brief    Checks whether the channel is busy serving a request.
       * 
       */
      bool is_active();

      /**
       * @brief    Checks whether the channel is being refreshed.
       * 
       */
      bool is_refresh();


    // Internal methods
    protected:
      void serve_completed_reads();
      void set_write_mode();
      bool find_request_to_schedule(ReqQueue*& queue, std::list<Request>::iterator& req, typename T::Command& cmd, std::vector<int>& addr_vec);
      void serve_pending_AP();
      // Update the activation count of a row
      void process_activate(const vector<int>& addr_vec);
      // Dump non-zero activation counts to file
      void dump_activation_count();
    protected:
      /**
       * @brief    Get the next command needed by a request by checking the prerequisites in the DRAM state machine.
       * 
       * @param    req            The request to get the next command from.
       * @return   T::Command     The next command to be issued in order to serve the request.
       */
      typename T::Command get_next_cmd(list<Request>::iterator req);

      /**
       * @brief    Issues the command to the DRAM hierarchy. Triggerring state and timing updates in the DRAM an updates the rowtable.
       * 
       * @param    cmd            The command.
       * @param    addr_vec       The address vector associated with the command.
       */
      virtual void issue_cmd(typename T::Command cmd, const vector<int>& addr_vec);


      /**
       * @brief    Upgrades the command to the autoprecharge (AP) version if applicable.
       * 
       * @details
       *  Depending on the row policy and the command type, a command can be upgraded to its autoprecharge (AP) version.
       *  Currently in the reference implementations, RD/WR commands will be upgraded to RDA/WRA when the ClosedAP policy is used.
       * 
       * @param    cmd            The command.
       * @param    addr_vec       The address vector associated with the command.
       */
      virtual void cmd_upgrade_AP(typename T::Command& cmd, const vector<int>& addr_vec);


    protected:
      void dump_trace(typename T::Command cmd, const vector<int>& addr_vec);
      void print_trace(typename T::Command cmd, const vector<int>& addr_vec);

    // Methods to initialize and finalize statistics
    public:
      /**
       * @brief    Registers the statistics associated with the memory controller.
       * 
       * @param    num_cores      The number of cores. Used to register the per-core stratistics.
       */
      virtual void reg_stats(uint num_cores);
      virtual void record_core(int coreid);
      virtual void finish(long read_req, long dram_cycles);

    // Statistics
    protected:
      // For counting bandwidth
      ScalarStat read_transaction_bytes;
      ScalarStat write_transaction_bytes;

      ScalarStat row_hits;
      ScalarStat row_misses;
      ScalarStat row_conflicts;
      VectorStat read_row_hits;
      VectorStat read_row_misses;
      VectorStat read_row_conflicts;
      VectorStat write_row_hits;
      VectorStat write_row_misses;
      VectorStat write_row_conflicts;
      ScalarStat preventive_refreshes;
      ScalarStat row_open_cycles_avg;
      ScalarStat row_open_cycles_sum;
      ScalarStat activate_commands;
      ScalarStat useless_activates;

      ScalarStat read_latency_avg;
      ScalarStat read_latency_sum;

      ScalarStat req_queue_length_avg;
      ScalarStat req_queue_length_sum;
      ScalarStat read_req_queue_length_avg;
      ScalarStat read_req_queue_length_sum;
      ScalarStat write_req_queue_length_avg;
      ScalarStat write_req_queue_length_sum;

    #ifndef INTEGRATED_WITH_GEM5
      VectorStat record_read_hits;
      VectorStat record_read_misses;
      VectorStat record_read_conflicts;
      VectorStat record_write_hits;
      VectorStat record_write_misses;
      VectorStat record_write_conflicts;
    #endif

    protected:
      void update_request_stats(std::list<Request>::iterator& req);
  };
}

#include "Controller.tpp"
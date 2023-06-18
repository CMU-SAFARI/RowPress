#include "Core.h"

namespace Ramulator
{
  Core::Core(const YAML::Node& config, Processor* processor, int coreid,
    std::string& trace_fname, function<bool(Request)> send_next,
    Cache* llc, MemoryBase* memory) : 
    id(coreid), llc(llc), processor(processor), trace(trace_fname), memory(memory)
  {
    trace.expected_limit_insts = processor->expected_limit_insts;

    // Build cache hierarchy
    if (processor->cachesys->no_core_caches) {
      send = send_next;
    } else {
      // L2 caches[0]
      caches.emplace_back(new Cache(processor->cachesys, Cache::Level::L2));
      // L1 caches[1]
      caches.emplace_back(new Cache(processor->cachesys, Cache::Level::L1));
      send = bind(&Cache::send, caches[1].get(), placeholders::_1);
      if (llc != nullptr) {
        caches[0]->concatlower(llc);
      }
      caches[1]->concatlower(caches[0].get());

      first_level_cache = caches[1].get();
    }
    if (processor->cachesys->no_core_caches) {
      more_reqs = trace.get_filtered_request(
          bubble_cnt, req_addr, req_type);
      req_addr = memory->translation->page_allocator(req_addr, id);
    } else {
      more_reqs = trace.get_unfiltered_request(
          bubble_cnt, req_addr, req_type);
      req_addr = memory->translation->page_allocator(req_addr, id);
    }

    
    // regStats
    record_cycs.name("record_cycs_core_" + to_string(id))
              .desc("Record cycle number for calculating weighted speedup. (Only valid when expected limit instruction number is non zero in config file.)")
              .precision(0)
              ;

    record_insts.name("record_insts_core_" + to_string(id))
                .desc("Retired instruction number when record cycle number. (Only valid when expected limit instruction number is non zero in config file.)")
                .precision(0)
                ;

    memory_access_cycles.name("memory_access_cycles_core_" + to_string(id))
                        .desc("memory access cycles in memory time domain")
                        .precision(0)
                        ;
    memory_access_cycles = 0;
    cpu_inst.name("cpu_instructions_core_" + to_string(id))
            .desc("cpu instruction number")
            .precision(0)
            ;
    cpu_inst = 0;
  }


  double Core::calc_ipc()
  {
      printf("[%d]retired: %ld, clk, %ld\n", id, retired, clk);
      return (double) retired / clk;
  }

  void Core::tick()
  {
      clk++;

      if(first_level_cache != nullptr)
          first_level_cache->tick();

      retired += window.retire();

      if (processor->expected_limit_insts == 0 && !more_reqs) return;

      // bubbles (non-memory operations)
      int inserted = 0;
      while (bubble_cnt > 0) {
          if (inserted == window.ipc) return;
          if (window.is_full()) return;

          window.insert(true, -1);
          inserted++;
          bubble_cnt--;
          cpu_inst++;
          if (uint64_t(cpu_inst.value()) == processor->expected_limit_insts && !reached_limit) {
            record_cycs = clk;
            record_insts = long(cpu_inst.value());
            memory->record_core(id);
            reached_limit = true;
          }
      }

      if (req_type == Request::Type::READ) {
          // read request
          if (inserted == window.ipc) return;
          if (window.is_full()) return;

          Request req(req_addr, req_type, callback, id);
          if (!send(req)) return;

          window.insert(false, req_addr);
          cpu_inst++;
      }
      else {
          // write request
          assert(req_type == Request::Type::WRITE);
          Request req(req_addr, req_type, callback, id);
          if (!send(req)) return;
          cpu_inst++;
      }
      if (uint64_t(cpu_inst.value()) == processor->expected_limit_insts && !reached_limit) {
        record_cycs = clk;
        record_insts = long(cpu_inst.value());
        memory->record_core(id);
        reached_limit = true;
      }

      if (processor->cachesys->no_core_caches) {
        more_reqs = trace.get_filtered_request(
            bubble_cnt, req_addr, req_type);
        if (req_addr != -1) {
          req_addr = memory->translation->page_allocator(req_addr, id);
        }
      } else {
        more_reqs = trace.get_unfiltered_request(
            bubble_cnt, req_addr, req_type);
        if (req_addr != -1) {
          req_addr = memory->translation->page_allocator(req_addr, id);
        }
      }
      if (!more_reqs) {
        if (!reached_limit) { // if the length of this trace is shorter than expected length, then record it when the whole trace finishes, and set reached_limit to true.
          // Hasan: overriding this behavior. We start the trace from the
          // beginning until the requested amount of instructions are
          // simulated. This should never be reached now.
          assert((processor->expected_limit_insts == 0) && "Shouldn't be reached when expected_limit_insts > 0 since we start over the trace");
          record_cycs = clk;
          record_insts = long(cpu_inst.value());
          memory->record_core(id);
          reached_limit = true;
        }
      }
  }

  bool Core::finished()
  {
    return !more_reqs && window.is_empty();
  }

  bool Core::has_reached_limit() {
    return reached_limit;
  }

  uint64_t Core::get_insts() {
      return long(cpu_inst.value());
  }

  void Core::receive(Request& req)
  {
      window.set_ready(req.addr, ~(l1_blocksz - 1l));
      if (req.arrive != -1 && req.depart > last) {
        memory_access_cycles += (req.depart - max(last, req.arrive));
        last = req.depart;
      }
  }

  void Core::reset_stats() {
      clk = 0;
      retired = 0;
      cpu_inst = 0;
  }

  bool Window::is_full()
  {
      return load == depth;
  }

  bool Window::is_empty()
  {
      return load == 0;
  }


  void Window::insert(bool ready, long addr)
  {
      assert(load <= depth);

      ready_list.at(head) = ready;
      addr_list.at(head) = addr;

      head = (head + 1) % depth;
      load++;
  }


  long Window::retire()
  {
      assert(load <= depth);

      if (load == 0) return 0;

      int retired = 0;
      while (load > 0 && retired < ipc) {
          if (!ready_list.at(tail))
              break;

          tail = (tail + 1) % depth;
          load--;
          retired++;
      }

      return retired;
  }


  void Window::set_ready(long addr, int mask)
  {
      if (load == 0) return;

      for (int i = 0; i < load; i++) {
          int index = (tail + i) % depth;
          if ((addr_list.at(index) & mask) != (addr & mask))
              continue;
          ready_list.at(index) = true;
      }
  }
}
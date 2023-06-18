#include "Processor.h"
#include <cassert>

using namespace std;
namespace Ramulator
{
  Processor* make_processor(const YAML::Node& config, MemoryBase* memory)
  {
    if (!config["processor"])
      throw std::runtime_error("Processor configuration (processor:) not found!");
    if (!config["memory"])
      throw std::runtime_error("Memory configuration (memory:) not found!");

    const YAML::Node& processor_config = config["processor"];
    const YAML::Node& memory_config = config["memory"];
    std::string processor_type = processor_config["type"].as<std::string>("");
    Processor* proc = nullptr;
    if      (processor_type == "")
      proc = new Processor(processor_config, memory_config, memory->send_fn, memory);
    else
      throw std::runtime_error(fmt::format("Unrecognized Processor type: {}", processor_type));

    // Register the statistics of the memory subsystem
    uint num_cores = proc->cores.size();
    memory->reg_stats(num_cores);

    return proc;
  }

  Processor::Processor(const YAML::Node& processor_config, 
    const YAML::Node& memory_config,
    function<bool(Request)> send_memory, MemoryBase* memory)
  {
    float memory_command_bus_period = memory_config["period"].as<float>(0);
    // warn the user
    if (memory_command_bus_period == 0)
      std::cout << "Warning: memory command bus speed is probably not set" << std::endl
        << "Ramulator won't output a reliable execution time in nanoseconds" << std::endl;
    float core_memory_period_ratio = processor_config["tick"].as<float>(0)/memory_config["tick"].as<float>(1);
    core_tick_period = memory_command_bus_period / core_memory_period_ratio;

    early_exit = processor_config["early_exit"].as<bool>(false);
    time_limit = processor_config["time_limit"].as<int>(0);
    warmup_insts = processor_config["warmup_insts"].as<size_t>(0);
    expected_limit_insts = processor_config["expected_limit_insts"].as<uint64_t>(0);

    std::vector<std::string> trace_list;
    if (processor_config["trace"])
    {
      trace_list = processor_config["trace"].as<std::vector<std::string>>();

      assert(trace_list.size() > 0);
      printf("Number of traces: %ld\n", trace_list.size());
      for (uint i = 0; i < trace_list.size(); ++i)      
        printf("trace_list[%d]: %s\n", i, trace_list[i].c_str());

      ipcs.resize(trace_list.size(), -1);
    }
    else
      assert(false && "Must have a trace list!");

    if (processor_config["cache"])
      cachesys = std::shared_ptr<CacheSystem>(new CacheSystem(processor_config["cache"], send_memory));
    assert(cachesys != nullptr);
    
    int tracenum = trace_list.size();
    if (cachesys->no_shared_cache) 
    {
      for (int i = 0 ; i < tracenum ; ++i) 
      {
        cores.emplace_back(new Core(processor_config, this, i, trace_list[i], 
          send_memory, nullptr, memory));
      }
    } 
    else 
    {
      llc = new Cache(cachesys, Cache::Level::L3);

      for (int i = 0 ; i < tracenum ; ++i) 
      {
        cores.emplace_back(new Core(processor_config, this, i, trace_list[i],
          std::bind(&Cache::send, llc, std::placeholders::_1),
          llc, memory));
      }
    }

    for (int i = 0 ; i < tracenum ; ++i) 
      cores[i]->callback = std::bind(&Processor::receive, this, placeholders::_1);

    // freq = Config::parse_frequency(config["frequency"].as<std::string>("4GHz"));
    tick_unit = processor_config["tick"].as<uint>(8);

    // regStats
    cpu_cycles.name("cpu_cycles")
              .desc("cpu cycle number")
              .precision(0)
              ;
    cpu_time.name("cpu_time")
            .desc("cpu time in nanoseconds")
            .precision(0)
            ;
    cpu_cycles = 0;
    cpu_time = 0;
  }

  void Processor::tick() {
    cpu_cycles++;
    cpu_time += core_tick_period;

    if((int(cpu_cycles.value()) % 50000000) == 0)
        printf("CPU heartbeat, cycles: %d \n", (int(cpu_cycles.value())));

    if (!(cachesys->no_core_caches && cachesys->no_shared_cache)) {
      cachesys->tick();
    }
    for (unsigned int i = 0 ; i < cores.size() ; ++i) {
      Core* core = cores[i].get();
      core->tick();
    }
  }

  void Processor::receive(Request& req) {
    if (!cachesys->no_shared_cache) {
      llc->callback(req);
    } else if (!cachesys->no_core_caches) {
      // Assume all cores have caches or don't have caches
      // at the same time.
      for (unsigned int i = 0 ; i < cores.size() ; ++i) {
        Core* core = cores[i].get();
        core->caches[0]->callback(req);
      }
    }
    for (unsigned int i = 0 ; i < cores.size() ; ++i) {
      Core* core = cores[i].get();
      core->receive(req);
    }
  }

  bool Processor::finished() {
    if (time_limit != 0)
    {
      if (cpu_time.value() > time_limit)
      {
        for (unsigned int i = 0 ; i < cores.size(); ++i) {
          if (ipcs[i] < 0) {
            ipcs[i] = cores[i]->calc_ipc();
            ipc += ipcs[i];
          }
        }        
        return true;
      }
      else
        return false;
    }
    else
    {
      if (early_exit) {
        for (unsigned int i = 0 ; i < cores.size(); ++i) {
          if (cores[i]->finished()) {
            for (unsigned int j = 0 ; j < cores.size() ; ++j) {
              ipc += cores[j]->calc_ipc();
            }
            return true;
          }
        }
        return false;
      } else {
        for (unsigned int i = 0 ; i < cores.size(); ++i) {
          if (!cores[i]->finished()) {
            return false;
          }
          if (ipcs[i] < 0) {
            ipcs[i] = cores[i]->calc_ipc();
            ipc += ipcs[i];
          }
        }
        return true;
      }
    }
  }

  void Processor::finalize() {
    // print finalize
    // cpu_time = cpu_cycles.value() * core_tick_period;
  }

  bool Processor::has_reached_limit() {
    if (time_limit != 0)
    {
      if (cpu_time.value() > time_limit)
        return true;
      else
        return false;
    }
    else
    {
      for (unsigned int i = 0 ; i < cores.size() ; ++i) {
        if (!cores[i]->has_reached_limit()) {
          return false;
        }
      }
      return true;
    }
  }

  uint64_t Processor::get_insts() {
      long insts_total = 0;
      for (unsigned int i = 0 ; i < cores.size(); i++) {
          insts_total += cores[i]->get_insts();
      }

      return insts_total;
  }

  void Processor::reset_stats() 
  {
    for (unsigned int i = 0 ; i < cores.size(); i++) 
      cores[i]->reset_stats();

    ipc = 0;
    cpu_time = 0;
    cpu_cycles = 0; // Should we?

    for (unsigned int i = 0; i < ipcs.size(); i++)
      ipcs[i] = -1;
  }
}
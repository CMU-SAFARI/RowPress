#pragma once

#include "Config.h"
#include "Memory.h"
#include "Processor.h"
#include "Standards.h"

namespace Ramulator
{
  void run_cputraces(const YAML::Node& config);

  void run_memorytraces(const YAML::Node& config);

  void warmup_processor(MemoryBase* mem, Processor* proc);

  void reload_options(const YAML::Node& config, MemoryBase* mem, Processor* proc);

  void start_sim_processor(MemoryBase* mem, Processor* proc);

  void start_sim_memory(MemoryBase* mem, std::string _trace);
}

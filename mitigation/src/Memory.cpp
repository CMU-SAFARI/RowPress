#include "Memory.h"

namespace Ramulator
{
  MemoryBase* make_memory(const YAML::Node& config)
  {
    try
    {
      if (!config["memory"])
        throw std::runtime_error("Memory configuration (memory:) not found!");

      const YAML::Node& memory_config = config["memory"];
      if (!memory_config["standard"])
        throw std::runtime_error("Memory standard (standard:) not found!");


      std::string standard = memory_config["standard"].as<std::string>("");
      MemoryBase* memory = nullptr;
      if      (standard == "DDR4")
        memory = new Memory<DDR4>(memory_config, make_DRAM_and_controllers<DDR4>(memory_config));
      else if (standard == "DDR5")
        memory = new Memory<DDR5>(memory_config, make_DRAM_and_controllers<DDR5>(memory_config));
      else
        throw std::runtime_error(fmt::format("Unknown memory standard {}.", standard));


      std::string stats_filename = standard;
      std::string stats_prefix = "";
      std::string stats_suffix = "";
      if (config["stats"])
      {
        stats_prefix = config["stats"]["prefix"].as<std::string>("");
        stats_suffix = config["stats"]["suffix"].as<std::string>("");
      }
      Stats::statlist.output(stats_prefix + standard + stats_suffix + ".stats");


      return memory;
    }
    catch(const std::runtime_error& e)
    {
      std::cerr << fmt::format("[Fatal] {}", e.what()) << std::endl;
      exit(-1);
    }
  }
}

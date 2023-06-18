#pragma once

#include "Scheduler.h"

namespace Ramulator
{
  template <class T>
  SchedulerBase<T>* make_scheduler(const YAML::Node& config, Controller<T>* ctrl)
  {
    if (! config["scheduler"])
      throw std::runtime_error("Request scheduler configuration (scheduler:) not found!");

    const YAML::Node& scheduler_config = config["scheduler"];
    std::string scheduler_type = scheduler_config["type"].as<std::string>("");

    SchedulerBase<T>* scheduler = nullptr;

    if      (scheduler_type == "FCFS")
      scheduler = new FCFS<T>(scheduler_config, ctrl);
    else if (scheduler_type == "FRFCFS")
      scheduler = new FRFCFS<T>(scheduler_config, ctrl);
    else if (scheduler_type == "FRFCFS_CAP")
      scheduler = new FRFCFS_CAP<T>(scheduler_config, ctrl);
    else if (scheduler_type == "FRFCFS_PHit")
      scheduler = new FRFCFS_PHit<T>(scheduler_config, ctrl);
    else
      throw std::runtime_error(fmt::format("Unrecognized Request scheduler type: {}", scheduler_type));

    std::cout << fmt::format("Request scheduler: {}", scheduler->to_string()) << std::endl;
    return scheduler;
  }

}

#pragma once

#include "RowPolicy.h"

namespace Ramulator
{
  template <class T>
  RowPolicyBase<T>* make_rowpolicy(const YAML::Node& config, Controller<T>* ctrl)
  {
    if (! config["row_policy"])
      throw std::runtime_error("Row Policy configuration (row_policy:) not found!");

    const YAML::Node& rowpolicy_config = config["row_policy"];
    std::string rowpolicy_type = rowpolicy_config["type"].as<std::string>("");

    RowPolicyBase<T>* rowpolicy = nullptr;

    if      (rowpolicy_type == "Closed")
      rowpolicy = new Closed<T>(rowpolicy_config, ctrl);
    else if (rowpolicy_type == "ClosedAP")
      rowpolicy = new ClosedAP<T>(rowpolicy_config, ctrl);
    else if (rowpolicy_type == "Opened")
      rowpolicy = new Opened<T>(rowpolicy_config, ctrl);        
    else if (rowpolicy_type == "TimeoutOpened")
      rowpolicy = new TimeoutOpened<T>(rowpolicy_config, ctrl);
    else if (rowpolicy_type == "ClosedDumb")
      rowpolicy = new ClosedDumb<T>(rowpolicy_config, ctrl);
    else
      throw std::runtime_error(fmt::format("Unrecognized Row Policy type: {}", rowpolicy_type));

    std::cout << fmt::format("Row Policy: {}", rowpolicy->to_string()) << std::endl;
    return rowpolicy;
  }

};
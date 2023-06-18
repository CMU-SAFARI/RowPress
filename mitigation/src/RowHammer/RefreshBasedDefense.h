#pragma once

#include "Controller.h"
#include "Config.h"

namespace Ramulator{

  template <class T>
  class Controller;

  template <class T>
  class RefreshBasedDefense {
    
    public:
      RefreshBasedDefense(const YAML::Node& config, Controller<T>* ctrl) { this->ctrl = ctrl; }
      virtual ~RefreshBasedDefense() = default;
      virtual std::string to_string() = 0;
      virtual void tick() = 0;
      virtual void update(typename T::Command cmd, const std::vector<int>& addr_vec, uint64_t open_for_nclocks) = 0;
    protected:
      virtual void schedule_preventive_refresh(std::vector<int> addr_vec) = 0;
      Controller<T>* ctrl;
  };
}

#include "RefreshBasedDefense.tpp"
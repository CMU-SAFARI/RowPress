#pragma once

#include "Controller.h"

namespace Ramulator
{
  template <typename T>
  class Controller;


  template <typename T>
  class RowPolicyBase
  {
    public:
      Controller<T>* ctrl;
    
    public:
      RowPolicyBase(const YAML::Node& config, Controller<T>* ctrl) : ctrl(ctrl) {};
      virtual ~RowPolicyBase() = default;
      virtual std::string to_string() = 0;

    public:
      virtual std::vector<int> get_victim(typename T::Command cmd) = 0;
  };


  template <typename T>
  class Closed : public RowPolicyBase<T>
  {
    using RowPolicyBase<T>::RowPolicyBase;
    public:
      std::string to_string()
      {
        return fmt::format("Closed\n"); 
      }

    public:
      std::vector<int> get_victim(typename T::Command cmd) override
      {
        for (auto& kv : this->ctrl->rowtable->table) 
        {
          if (!this->ctrl->is_ready(cmd, kv.first))
              continue;
          return kv.first;
        }
        return std::vector<int>();
      }
  };


  template <typename T>
  class ClosedDumb : public RowPolicyBase<T>
  {
    using RowPolicyBase<T>::RowPolicyBase;
    public:
      std::string to_string()
      {
        return fmt::format("Closed Dumb\n"); 
      }

    public:
      std::vector<int> get_victim(typename T::Command cmd) override
      {
        for (auto& kv : this->ctrl->rowtable->table) 
        {
          if (!this->ctrl->is_ready(cmd, kv.first))
              continue;
          return kv.first;
        }
        return std::vector<int>();
      }
  };


  template <typename T>
  class ClosedAP : public RowPolicyBase<T>
  {
    using RowPolicyBase<T>::RowPolicyBase;
    public:
      std::string to_string()
      {
        return fmt::format("ClosedAP\n"); 
      }

    public:
      std::vector<int> get_victim(typename T::Command cmd) override
      {
        for (auto& kv : this->ctrl->rowtable->table) 
        {
          if (!this->ctrl->is_ready(cmd, kv.first))
              continue;
          return kv.first;
        }
        return std::vector<int>();
      }
  };


  template <typename T>
  class Opened : public RowPolicyBase<T>
  {
    using RowPolicyBase<T>::RowPolicyBase;
    public:
      std::string to_string()
      {
        return fmt::format("Opened\n"); 
      }

    public:
      std::vector<int> get_victim(typename T::Command cmd) override { return vector<int>(); }
  };


  template <typename T>
  class TimeoutOpened : public RowPolicyBase<T>
  {
    public:
      uint timeout = 50;

    public:
      TimeoutOpened(const YAML::Node& config, Controller<T>* ctrl):
      RowPolicyBase<T>(config, ctrl)
      {
        timeout = config["timeout"].as<uint>(50);
      };

      std::string to_string()
      {
        return fmt::format("TimeoutOpened\n"
                           "  └  "
                           "timeout = {}\n", timeout); 
      }
    public:
      std::vector<int> get_victim(typename T::Command cmd) override
      {
        for (auto& kv : this->ctrl->rowtable->table) 
        {
          auto& entry = kv.second;
          if (this->ctrl->clk - entry.timestamp < timeout)
            continue;
          if (!this->ctrl->is_ready(cmd, kv.first))
            continue;
          return kv.first;
        }
        return std::vector<int>();
      }
  };
}

#include "RowPolicy.tpp"
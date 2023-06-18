#pragma once

#include "Controller.h"

namespace Ramulator
{
  template <typename T>
  class Controller;


  template <typename T>
  class RowTable
  {
    public:
      Controller<T>* ctrl;

      struct Entry 
      {
        int row;
        int hits;
        uint64_t timestamp;
        uint64_t open_since;
      };

      std::map<std::vector<int>, Entry> table;


    public:
      RowTable(const YAML::Node& config, Controller<T>* ctrl) : ctrl(ctrl) {};
      virtual ~RowTable() = default;
      virtual std::string to_string();

    public:
      virtual void update(typename T::Command cmd, const vector<int>& addr_vec, uint64_t clk);

      virtual int get_hits(const vector<int>& addr_vec, const bool to_opened_row = false);
      virtual int get_open_row(const vector<int>& addr_vec);
      virtual int get_open_time(const vector<int>& addr_vec, uint64_t clk);
  };
}

#include "RowTable.tpp"
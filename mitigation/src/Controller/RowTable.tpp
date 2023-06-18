#pragma once

#include "RowTable.h"

namespace Ramulator
{
  template <class T>
  RowTable<T>* make_rowtable(const YAML::Node& config, Controller<T>* ctrl)
  {
    RowTable<T>* rowtable = nullptr;

    if (! config["rowtable"])
      rowtable = new RowTable<T>(config, ctrl);
    else
    {
      const YAML::Node& rowtable_config = config["rowtable"];
      std::string rowtable_type = rowtable_config["type"].as<std::string>("");

      if      (rowtable_type == "")
        rowtable = new RowTable<T>(config, ctrl);
      else
        throw std::runtime_error(fmt::format("Unrecognized Row Table type: {}", rowtable_type));
    }

    if (rowtable->to_string() != "")
      std::cout << fmt::format("Row Table: {}", rowtable->to_string()) << std::endl;
    return rowtable;
  }


  template <typename T>
  std::string RowTable<T>::to_string()
  {
    return fmt::format(""); 
  };

  template <typename T>
  void RowTable<T>::update(typename T::Command cmd, const vector<int>& addr_vec, uint64_t clk)
  {
    auto begin = addr_vec.begin();
    auto end = begin + int(T::Level::Row);
    vector<int> rowgroup(begin, end); // bank or subarray
    int row = *end;

    T* spec = ctrl->channel->spec;

    if (spec->is_opening(cmd))
        table.insert({rowgroup, {row, 0, clk, clk}});

    if (spec->is_accessing(cmd)) {
        // we are accessing a row -- update its entry
        auto match = table.find(rowgroup);
        assert(match != table.end());
        assert(match->second.row == row);
        match->second.hits++;
        match->second.timestamp = clk;
    } /* accessing */

    if (spec->is_closing(cmd)) 
    {
      // we are closing one or more rows -- remove their entries
      int n_rm = 0;
      int scope;
      if (spec->is_accessing(cmd))
        scope = int(T::Level::Row) - 1; //special condition for RDA and WRA
      else
        scope = int(spec->scope[int(cmd)]);

      for (auto it = table.begin(); it != table.end();) 
      {
        if (equal(begin, begin + scope + 1, it->first.begin())) 
        {
          n_rm++;
          it = table.erase(it);
        }
        else
          it++;
      }
      assert(n_rm > 0);
    }
  }

  template <typename T>
  int RowTable<T>::get_hits(const vector<int>& addr_vec, const bool to_opened_row)
  {
    auto begin = addr_vec.begin();
    auto end = begin + int(T::Level::Row);

    vector<int> rowgroup(begin, end);
    int row = *end;

    auto itr = table.find(rowgroup);
    if (itr == table.end())
        return 0;

    if(!to_opened_row && (itr->second.row != row))
        return 0;

    return itr->second.hits;
  }

  template <typename T>
  int RowTable<T>::get_open_row(const vector<int>& addr_vec) 
  {
    auto begin = addr_vec.begin();
    auto end = begin + int(T::Level::Row);

    vector<int> rowgroup(begin, end);

    auto itr = table.find(rowgroup);
    if(itr == table.end())
        return -1;

    return itr->second.row;
  }

  template <typename T>
  int RowTable<T>::get_open_time(const vector<int>& addr_vec, uint64_t clk) 
  {
    auto begin = addr_vec.begin();
    auto end = begin + int(T::Level::Row);

    vector<int> rowgroup(begin, end);

    auto itr = table.find(rowgroup);
    if(itr == table.end())
        return -1;

    return clk - itr->second.open_since;
  }


};
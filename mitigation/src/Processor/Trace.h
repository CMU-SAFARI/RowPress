#pragma once

#include <iostream>
#include <fstream>
#include <string>
#include <exception>

#include "Request.h"

namespace Ramulator
{
  class Trace 
  {
    public:
      Trace(std::string& trace_fname);
      
      // trace file format 1:
      // [# of bubbles(non-mem instructions)] [read address(dec or hex)] <optional: write address(evicted cacheline)>
      bool get_unfiltered_request(long& bubble_cnt, long& req_addr, Request::Type& req_type);
      bool get_filtered_request(long& bubble_cnt, long& req_addr, Request::Type& req_type);

      // trace file format 2:
      // [address(hex)] [R/W]
      bool get_dramtrace_request(long& req_addr, Request::Type& req_type);

      long expected_limit_insts = 0;

    private:
      std::ifstream file;
      std::string trace_name;
  };

}
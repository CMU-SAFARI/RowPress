#include "Trace.h"

namespace Ramulator
{
    Trace::Trace(std::string& trace_fname):
  file(trace_fname), trace_name(trace_fname)
  {
    if (!file.good()) 
    {
      std::cerr << "Bad trace file: " << trace_fname << std::endl;
      exit(1);
    }
  }

  bool Trace::get_unfiltered_request(long& bubble_cnt, long& req_addr, Request::Type& req_type)
  {
      std::string line;
      std::getline(file, line);
      if (file.eof()) {
        file.clear();
        file.seekg(0, file.beg);
        std::getline(file, line);
        //return false;
      }
      size_t pos, end;
      bubble_cnt = std::stoul(line, &pos, 10);
      pos = line.find_first_not_of(' ', pos+1);
      req_addr = std::stoul(line.substr(pos), &end, 0);

      pos = line.find_first_not_of(' ', pos+end);

      if (pos == std::string::npos || line.substr(pos)[0] == 'R')
      {
        req_type = Request::Type::READ;
      }
      else if (line.substr(pos)[0] == 'W')
      {
        req_type = Request::Type::WRITE;
      }
      else
        throw std::runtime_error("Unrecognized request type!");
      return true;
  }

  bool Trace::get_filtered_request(long& bubble_cnt, long& req_addr, Request::Type& req_type)
  {
      static bool has_write = false;
      static long write_addr;
      static int line_num = 0;
      if (has_write){
          bubble_cnt = 0;
          req_addr = write_addr;
          req_type = Request::Type::WRITE;
          has_write = false;
          return true;
      }
      std::string line;
      std::getline(file, line);
      line_num ++;
      if (file.eof() || line.size() == 0) {
          file.clear();
          file.seekg(0, file.beg);
          line_num = 0;

          if(expected_limit_insts == 0) {
              has_write = false;
              return false;
          }
          else { // starting over the input trace file
              std::getline(file, line);
              line_num++;
          }
      }

      size_t pos, end;
      bubble_cnt = std::stoul(line, &pos, 10);

      pos = line.find_first_not_of(' ', pos+1);
      req_addr = stoul(line.substr(pos), &end, 0);
      req_type = Request::Type::READ;

      pos = line.find_first_not_of(' ', pos+end);
      if (pos != std::string::npos){
          has_write = true;
          write_addr = std::stoul(line.substr(pos), NULL, 0);
      }
      return true;
  }

  bool Trace::get_dramtrace_request(long& req_addr, Request::Type& req_type)
  {
      std::string line;
      std::getline(file, line);
      if (file.eof()) {
          return false;
      }
      size_t pos;
      req_addr = std::stoul(line, &pos, 16);

      pos = line.find_first_not_of(' ', pos+1);

      if (pos == std::string::npos || line.substr(pos)[0] == 'R')
          req_type = Request::Type::READ;
      else if (line.substr(pos)[0] == 'W')
          req_type = Request::Type::WRITE;
      else
          throw std::runtime_error("Unrecognized request type!");
      return true;
  }

};
#ifndef __REQUEST_H
#define __REQUEST_H

#include <vector>
#include <functional>

namespace Ramulator
{
  class Request
  {
    public:
      bool stats_updated;
      long addr;
      // long addr_row;
      std::vector<int> addr_vec;

      // specify which core this request sent from, for virtual address translation
      int coreid;

      enum class Type
      {
        READ,     WRITE,
        REFRESH,  REFRESH_B, 
        RFM,      RFM_B,
        SELFREFRESH,
        POWERDOWN,
        PREVENTIVE_REFRESH,
        EXTENSION,
        MAX
      } type;

      int extension_id = -1;

      long arrive = -1;
      long depart = -1;
      std::function<void(Request&)> callback; // call back with more info

      Request(long addr, Type type, int coreid = 0)
        : stats_updated(false), addr(addr), coreid(coreid), type(type),
      callback([](Request& req){}) {}

      Request(long addr, Type type, std::function<void(Request&)> callback, int coreid = 0)
        : stats_updated(false), addr(addr), coreid(coreid), type(type), callback(callback) {}

      Request(std::vector<int>& addr_vec, Type type, std::function<void(Request&)> callback, int coreid = 0)
        : stats_updated(false), addr_vec(addr_vec), coreid(coreid), type(type), callback(callback) {}

      Request()
        : stats_updated(false), coreid(0) {}
  };

} /*namespace Ramulator*/

#endif /*__REQUEST_H*/


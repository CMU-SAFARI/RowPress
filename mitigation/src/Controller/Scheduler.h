/***************************** SCHEDULER.H ***********************************
- SAFARI GROUP

This file contains the different scheduling policies and row policies that the 
memory controller can use to schedule requests.

Current Memory Scheduling Policies:

1) FCFS - First Come First Serve
        This scheduling policy schedules memory requests chronologically

2) FRFCFS - Frist Ready First Come First Serve
        This scheduling policy first checks if a request is READY(meets all 
        timing parameters), if yes then it is prioritized. If multiple requests
        are ready, they they are scheduled chronologically. Otherwise, it 
        behaves the same way as FCFS. 

3) FRFCFS_Cap - First Ready First Come First Serve Cap
       This scheduling policy behaves the same way as FRFCS, except that it has
       a cap on the number of hits you can get in a certain row. The CAP VALUE
       can be altered by changing the number for the "cap" variable in 
       line number 76. 

4) FRFCFS_PriorHit - First Ready First Come First Serve Prioritize Hits
       This scheduling policy behaves the same way as FRFCFS, except that it
       prioritizes row hits more than readiness. 

You can select which scheduler you want to use by changing the value of 
"type" variable on line number 74.

                _______________________________________

Current Row Policies:

1) Closed   - Precharges a row as soon as there are no pending references to 
              the active row.
2) ClosedAP - Closed Auto Precharge
3) Opened   - Precharges a row only if there are pending references to 
              other rows.
4) Timeout  - Precharges a row after X time if there are no pending references.
              'X' time can be changed by changing the variable timeout 
              on line number 221

*****************************************************************************/

#pragma once

#include <vector>
#include <map>
#include <list>
#include <functional>
#include <cassert>

#include "DRAM.h"
#include "Request.h"
#include "Controller.h"

using namespace std;

namespace Ramulator
{
  template <typename T>
  class Controller;


  template <typename T>
  class SchedulerBase
  {
    public:
      Controller<T>* ctrl;


    public:
      SchedulerBase(const YAML::Node& config, Controller<T>* ctrl) : ctrl(ctrl) {};
      virtual ~SchedulerBase() = default;
      virtual std::string to_string() = 0;

    public:
      virtual std::list<Request>::iterator get_head(std::list<Request>& q)
      {
        //If queue is empty, return end of queue
        if (!q.size())
          return q.end();

        //Else return based on the policy
        auto head = q.begin();
        for (auto itr = next(q.begin(), 1); itr != q.end(); itr++)
          head = compare(head, itr);

        return head;
      }

    protected:
      virtual std::list<Request>::iterator compare(std::list<Request>::iterator req1, std::list<Request>::iterator req2) = 0;
  };


  template <typename T>
  class FCFS: public SchedulerBase<T>
  {
    using SchedulerBase<T>::SchedulerBase;
    public:
      std::string to_string() 
      { 
        return fmt::format("FCFS\n"); 
      };

    protected:
      std::list<Request>::iterator compare(std::list<Request>::iterator req1, std::list<Request>::iterator req2) override
      {
        if (req1->arrive <= req2->arrive) return req1;
        else return req2;
      };
  };


  template <typename T>
  class FRFCFS: public SchedulerBase<T>
  {
    using SchedulerBase<T>::SchedulerBase;
    public:
      std::string to_string() 
      { 
        return fmt::format("FRFCFS\n"); 
      };

    protected:
      std::list<Request>::iterator compare(std::list<Request>::iterator req1, std::list<Request>::iterator req2) override
      {
        bool ready1 = this->ctrl->is_ready(req1);
        bool ready2 = this->ctrl->is_ready(req2);

        if (ready1 ^ ready2) 
        {
          if (ready1) return req1;
          else return req2;
        }

        if (req1->arrive <= req2->arrive) return req1;
        else return req2;
      };
  };


  template <typename T>
  class FRFCFS_CAP: public SchedulerBase<T>
  {
    public:
      int cap = 16;

    public:
      FRFCFS_CAP(const YAML::Node& config, Controller<T>* ctrl):
      SchedulerBase<T>(config, ctrl)
      {
        cap = config["cap"].as<int>(16);
      };

      std::string to_string() 
      { 
        return fmt::format("FRFCFS_CAP\n"
                           "  └  "
                           "cap = {}\n", cap); 
      };

    protected:
      std::list<Request>::iterator compare(std::list<Request>::iterator req1, std::list<Request>::iterator req2) override
      {
        bool ready1 = this->ctrl->is_ready(req1);
        bool ready2 = this->ctrl->is_ready(req2);

        ready1 = ready1 && (this->ctrl->rowtable->get_hits(req1->addr_vec) <= cap);
        ready2 = ready2 && (this->ctrl->rowtable->get_hits(req2->addr_vec) <= cap);

        if (ready1 ^ ready2) 
        {
          if (ready1) return req1;
          else return req2;
        }

        if (req1->arrive <= req2->arrive) return req1;
        else return req2;
      };
  };


  template <typename T>
  class FRFCFS_PHit: public SchedulerBase<T>
  {
    using SchedulerBase<T>::SchedulerBase;
    public:
      std::string to_string() 
      { 
        return fmt::format("FRFCFS_PHit\n"); 
      };

    protected:
      std::list<Request>::iterator get_head(std::list<Request>& q) override
      {
        //If queue is empty, return end of queue
        if (!q.size())
          return q.end();

        //Else return based on FRFCFS_PriorHit Scheduling Policy
        auto head = q.begin();
        for (auto itr = next(q.begin(), 1); itr != q.end(); itr++)
          head = compare(head, itr);

        if (this->ctrl->is_ready(head) && this->ctrl->is_row_hit(head))
            return head;

        // prepare a list of hit request
        std::vector<std::vector<int>> hit_reqs;
        for (auto itr = q.begin() ; itr != q.end() ; ++itr) 
        {
          if (this->ctrl->is_row_hit(itr)) 
          {
            auto begin = itr->addr_vec.begin();
            // TODO Here it assumes all DRAM standards use PRE to close a row
            // It's better to make it more general.
            auto end = begin + int(this->ctrl->channel->spec->scope[int(T::Command::PRE)]) + 1;
            std::vector<int> rowgroup(begin, end); // bank or subarray
            hit_reqs.push_back(rowgroup);
          }
        }
        // if we can't find proper request, we need to return q.end(),
        // so that no command will be scheduled
        head = q.end();
        for (auto itr = q.begin(); itr != q.end(); itr++) 
        {
          bool violate_hit = false;
          if ((!this->ctrl->is_row_hit(itr)) && this->ctrl->is_row_open(itr)) 
          {
            // so the next instruction to be scheduled is PRE, might violate hit
            auto begin = itr->addr_vec.begin();
            // TODO Here it assumes all DRAM standards use PRE to close a row
            // It's better to make it more general.
            auto end = begin + int(this->ctrl->channel->spec->scope[int(T::Command::PRE)]) + 1;
            vector<int> rowgroup(begin, end); // bank or subarray
            for (const auto& hit_req_rowgroup : hit_reqs) 
            {
              if (rowgroup == hit_req_rowgroup) 
              {
                violate_hit = true;
                break;
              }  
            }
          }
          if (violate_hit)
              continue;

          // If it comes here, that means it won't violate any hit request
          if (head == q.end())
            head = itr;
          else
            head = compare(head, itr);
        }

        return head;
      };

      std::list<Request>::iterator compare(std::list<Request>::iterator req1, std::list<Request>::iterator req2) override
      {
        bool ready1 = this->ctrl->is_ready(req1) && this->ctrl->is_row_hit(req1);
        bool ready2 = this->ctrl->is_ready(req2) && this->ctrl->is_row_hit(req2);

        if (ready1 ^ ready2) 
        {
          if (ready1) return req1;
          else return req2;
        }

        if (req1->arrive <= req2->arrive) return req1;
        else return req2;
      };
  };

}

#include "Scheduler.tpp"
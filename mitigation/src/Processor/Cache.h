#ifndef __CACHE_H
#define __CACHE_H

#include "Config.h"
#include "Request.h"
#include "Statistics.h"

#include <algorithm>
#include <cstdio>
#include <cassert>
#include <functional>
#include <list>
#include <map>
#include <memory>
#include <queue>
#include <list>

namespace Ramulator
{
  class CacheSystem;

  class Cache 
  {
    public:
      enum class Level : uint { L1, L2, L3, MAX };
      inline static const std::string level_strings[uint(Level::MAX)] = {"L1", "L2", "L3"};

      Cache(std::shared_ptr<CacheSystem> cachesys, Level level);

      // // Latency of each level
      // int latency_each[uint(Level::MAX)] = {4, 12, 31};
      // // Accumulated latencies at each level
      // int latency[uint(Level::MAX)] = {4, 4 + 12, 4 + 12 + 31};

      uint latency = 0;
      uint aggregated_latency = 0;

      std::shared_ptr<CacheSystem> cachesys;
      std::vector<Cache*> higher_caches;
      Cache* lower_cache;

      struct Line 
      {
        uint64_t addr;
        uint64_t tag;
        bool dirty;

        // When the lock is on, the value is not valid yet.
        bool lock; 

        Line(uint64_t addr, uint64_t tag):
          addr(addr), tag(tag), dirty(false), lock(true) {};
        Line(uint64_t addr, uint64_t tag, bool lock, bool dirty):
          addr(addr), tag(tag), dirty(dirty), lock(lock) {};
      };

      void tick();
      bool send(Request req);
      void concatlower(Cache* lower);
      void callback(Request& req);

    protected:
      const Level level;
      bool is_first_level;
      bool is_last_level;

      size_t size;

      uint assoc;
      uint block_num;
      uint index_mask;
      uint block_size;
      uint index_offset;
      uint tag_offset;
      uint mshr_entry_num;

      std::vector<std::pair<uint64_t, std::list<Line>::iterator>> mshr_entries;
      std::list<Request> retry_list;

      std::map<int, std::list<Line> > cache_lines;

    protected:
      int calc_log2(int val) 
      {
        int n = 0;
        while ((val >>= 1))
          n ++;
        return n;
      }

      int get_index(uint64_t addr) { return (addr >> index_offset) & index_mask; };

      uint64_t get_tag(uint64_t addr) { return (addr >> tag_offset); };

      // Align the address to cache line size
      uint64_t align(uint64_t addr) { return (addr & ~(block_size-1l)); };

      // Evict the cache line from higher level to this level.
      // Pass the dirty bit and update LRU queue.
      void evictline(uint64_t addr, bool dirty);

      // Invalidate the line from this level to higher levels
      // The return value is a pair. The first element is invalidation
      // latency, and the second is wether the value has new version
      // in higher level and this level.
      std::pair<uint64_t, bool> invalidate(uint64_t addr);

      // Evict the victim from current set of lines.
      // First do invalidation, then call evictline(L1 or L2) or send
      // a write request to memory(L3) when dirty bit is on.
      void evict(std::list<Line>* lines, std::list<Line>::iterator victim);

      // First test whether need eviction, if so, do eviction by
      // calling evict function. Then allocate a new line and return
      // the iterator points to it.
      std::list<Line>::iterator allocate_line(std::list<Line>& lines, uint64_t addr);

      // Check whether the set to hold addr has space or eviction is needed.
      bool need_eviction(const std::list<Line>& lines, uint64_t addr);

      // Check whether this addr is hit and fill in the pos_ptr with
      // the iterator to the hit line or lines.end()
      bool is_hit(std::list<Line>& lines, uint64_t addr,
                  std::list<Line>::iterator* pos_ptr);

      bool all_sets_locked(const std::list<Line>& lines);

      bool check_unlock(uint64_t addr);

      std::vector<std::pair<uint64_t, std::list<Line>::iterator>>::iterator
      hit_mshr(uint64_t addr);

      std::list<Line>& get_lines(uint64_t addr);

      protected:
        ScalarStat cache_read_miss;
        ScalarStat cache_write_miss;
        ScalarStat cache_total_miss;
        ScalarStat cache_eviction;
        ScalarStat cache_read_access;
        ScalarStat cache_write_access;
        ScalarStat cache_total_access;
        ScalarStat cache_mshr_hit;
        ScalarStat cache_mshr_unavailable;
        ScalarStat cache_set_unavailable;
  };

  class CacheSystem 
  {
    public:
      // wait_list contains miss requests with their latencies in
      // cache. When this latency is met, the send_memory function
      // will be called to send the request to the memory system.
      std::list<std::pair<uint64_t, Request>> wait_list;

      // hit_list contains hit requests with their latencies in cache.
      // callback function will be called when this latency is met and
      // set the instruction status to ready in processor's window.
      std::list<std::pair<uint64_t, Request>> hit_list;

      std::function<bool(Request)> send_memory;

      Cache::Level first_level;
      Cache::Level last_level;

      uint64_t clk = 0;
      void tick();

      size_t capacity_each[uint(Cache::Level::MAX)] = {0};
      size_t assoc_each[uint(Cache::Level::MAX)] = {0};
      size_t mshr_num_each[uint(Cache::Level::MAX)] = {0};
      uint   latency_each[uint(Cache::Level::MAX)] = {0};

      bool no_core_caches = false;
      bool no_shared_cache = false;

      CacheSystem(const YAML::Node& config, std::function<bool(Request)> send_memory);

    protected:
      const YAML::Node& config;
  };
} // namespace Ramulator

#endif /* __CACHE_H */

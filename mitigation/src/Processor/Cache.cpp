#include "Cache.h"

#ifndef DEBUG_CACHE
#define debug(...)
#else
#define debug(...) do { \
          printf("\033[36m[DEBUG] %s ", __FUNCTION__); \
          printf(__VA_ARGS__); \
          printf("\033[0m\n"); \
      } while (0)
#endif

namespace Ramulator
{
  Cache::Cache(std::shared_ptr<CacheSystem> cachesys, Level level):
  cachesys(cachesys), higher_caches(0), lower_cache(nullptr), level(level), block_size(64)
  {
    debug("level %d size %d assoc %d block_size %d\n",
        int(level), size, assoc, block_size);

    is_first_level = (level == cachesys->first_level);
    is_last_level = (level == cachesys->last_level);

    // Initialize cache configuration
    size = cachesys->capacity_each[uint(level)];
    assoc = cachesys->assoc_each[uint(level)];
    mshr_entry_num = cachesys->mshr_num_each[uint(level)];

    block_num = size / (block_size * assoc);
    index_mask = block_num - 1;
    index_offset = calc_log2(block_size);
    tag_offset = calc_log2(block_num) + index_offset;

    latency = cachesys->latency_each[uint(level)];
    for (uint i = 0; i < uint(level) + 1; i++)
      aggregated_latency += cachesys->latency_each[i];

    debug("index_offset %d", index_offset);
    debug("index_mask 0x%x", index_mask);
    debug("tag_offset %d", tag_offset);

    // regStats
    std::string level_string = level_strings[uint(level)];
    cache_read_miss.name(level_string + std::string("_cache_read_miss"))
                  .desc("cache read miss count")
                  .precision(0)
                  ;

    cache_write_miss.name(level_string + std::string("_cache_write_miss"))
                    .desc("cache write miss count")
                    .precision(0)
                    ;

    cache_total_miss.name(level_string + std::string("_cache_total_miss"))
                    .desc("cache total miss count")
                    .precision(0)
                    ;

    cache_eviction.name(level_string + std::string("_cache_eviction"))
                  .desc("number of evict from this level to lower level")
                  .precision(0)
                  ;

    cache_read_access.name(level_string + std::string("_cache_read_access"))
                    .desc("cache read access count")
                    .precision(0)
                    ;

    cache_write_access.name(level_string + std::string("_cache_write_access"))
                      .desc("cache write access count")
                      .precision(0)
                      ;

    cache_total_access.name(level_string + std::string("_cache_total_access"))
                      .desc("cache total access count")
                      .precision(0)
                      ;

    cache_mshr_hit.name(level_string + std::string("_cache_mshr_hit"))
                  .desc("cache mshr hit count")
                  .precision(0)
                  ;
    cache_mshr_unavailable.name(level_string + std::string("_cache_mshr_unavailable"))
                          .desc("cache mshr not available count")
                          .precision(0)
                          ;
    cache_set_unavailable.name(level_string + std::string("_cache_set_unavailable"))
                          .desc("cache set not available")
                          .precision(0)
                          ;
  }

  bool Cache::send(Request req) 
  {
    debug("level %d req.addr %lx req.type %d, index %d, tag %ld",
        int(level), req.addr, int(req.type), get_index(req.addr),
        get_tag(req.addr));

    cache_total_access++;
    if (req.type == Request::Type::WRITE)
      cache_write_access++;
    else
      assert(req.type == Request::Type::READ);
      cache_read_access++;

    // If there isn't a set, create it.
    auto& lines = get_lines(req.addr);
    std::list<Line>::iterator line;

    if (is_hit(lines, req.addr, &line)) 
    {
      lines.push_back(Line(req.addr, get_tag(req.addr), false,
          line->dirty || (req.type == Request::Type::WRITE)));
      lines.erase(line);
      cachesys->hit_list.push_back(
        std::make_pair(cachesys->clk + aggregated_latency, req));

      debug("hit, update timestamp %ld", cachesys->clk);
      debug("hit finish time %ld",
        cachesys->clk + aggregated_latency);

      return true;
    } 
    else 
    {
      debug("miss @level %d", int(level));
      cache_total_miss++;
      if (req.type == Request::Type::WRITE)
        cache_write_miss++;
      else
        assert(req.type == Request::Type::READ);
        cache_read_miss++;

      // The dirty bit will be set if this is a write request and @L1
      bool dirty = (req.type == Request::Type::WRITE);

      // Modify the type of the request to lower level
      if (req.type == Request::Type::WRITE)
        req.type = Request::Type::READ;

      // Look it up in MSHR entries
      assert(req.type == Request::Type::READ);
      auto mshr = hit_mshr(req.addr);
      if (mshr != mshr_entries.end())
      {
        debug("hit mshr");
        cache_mshr_hit++;
        mshr->second->dirty = dirty || mshr->second->dirty;
        return true;
      }

      // All requests come to this stage will be READ, so they
      // should be recorded in MSHR entries.
      if (mshr_entries.size() == mshr_entry_num) 
      {
        // When no MSHR entries available, the miss request
        // is stalling.
        cache_mshr_unavailable++;
        debug("no mshr entry available");
        return false;
      }

      // Check whether there is a line available
      if (all_sets_locked(lines)) 
      {
        cache_set_unavailable++;
        return false;
      }

      auto newline = allocate_line(lines, req.addr);
      if (newline == lines.end())
        return false;

      newline->dirty = dirty;

      // Add to MSHR entries
      mshr_entries.push_back(make_pair(req.addr, newline));

      // Send the request to next level;
      if (!is_last_level) {
        if(!lower_cache->send(req))
          retry_list.push_back(req);
      } 
      else 
      {
        cachesys->wait_list.push_back(
          std::make_pair(cachesys->clk + aggregated_latency, req));
      }
      return true;
    }
  }

  void Cache::evictline(uint64_t addr, bool dirty) 
  {
    auto it = cache_lines.find(get_index(addr));
    assert(it != cache_lines.end()); // check inclusive cache
    auto& lines = it->second;
    auto line = find_if(lines.begin(), lines.end(),
        [addr, this](Line l){return (l.tag == get_tag(addr));});

    assert(line != lines.end());
    // Update LRU queue. The dirty bit will be set if the dirty
    // bit inherited from higher level(s) is set.
    lines.push_back(Line(addr, get_tag(addr), false,
        dirty || line->dirty));
    lines.erase(line);
  }

  std::pair<uint64_t, bool> Cache::invalidate(uint64_t addr) 
  {
    uint64_t delay = latency;
    bool dirty = false;

    auto& lines = get_lines(addr);
    if (lines.size() == 0)
      // The line of this address doesn't exist.
      return std::make_pair(0, false);

    auto line = find_if(lines.begin(), lines.end(),
        [addr, this](Line l){return (l.tag == get_tag(addr));});

    // If the line is in this level cache, then erase it from
    // the buffer.
    if (line != lines.end()) 
    {
      assert(!line->lock);
      debug("invalidate %lx @ level %d", addr, int(level));
      lines.erase(line);
    } 
    else 
      // If it's not in current level, then no need to go up.
      return std::make_pair(delay, false);

    if (higher_caches.size()) 
    {
      uint64_t max_delay = delay;
      for (auto hc : higher_caches) 
      {
        auto result = hc->invalidate(addr);
        if (result.second)
          max_delay = std::max(max_delay, delay + result.first * 2);
        else
          max_delay = std::max(max_delay, delay + result.first);
        dirty = dirty || line->dirty || result.second;
      }
      delay = max_delay;
    } 
    else
      dirty = line->dirty;

    return std::make_pair(delay, dirty);
  }


  void Cache::evict(std::list<Line>* lines, std::list<Line>::iterator victim) 
  {
    debug("level %d miss evict victim %lx", int(level), victim->addr);
    cache_eviction++;

    uint64_t addr = victim->addr;
    uint64_t invalidate_time = 0;
    bool dirty = victim->dirty;

    // First invalidate the victim line in higher level.
    if (higher_caches.size()) 
    {
      for (auto hc : higher_caches) 
      {
        auto result = hc->invalidate(addr);
        invalidate_time = std::max(invalidate_time,
            result.first + (result.second ? latency : 0));
        dirty = dirty || result.second || victim->dirty;
      }
    }

    debug("invalidate delay: %ld, dirty: %s", invalidate_time,
        dirty ? "true" : "false");

    if (!is_last_level) 
    {
      // not LLC eviction
      assert(lower_cache != nullptr);
      lower_cache->evictline(addr, dirty);
    }
    else 
    {
      // LLC eviction
      if (dirty) 
      {
        Request write_req(addr, Request::Type::WRITE);
        cachesys->wait_list.push_back(std::make_pair(
            cachesys->clk + invalidate_time + aggregated_latency,
            write_req));

        debug("inject one write request to memory system "
            "addr %lx, invalidate time %ld, issue time %ld",
            write_req.addr, invalidate_time,
            cachesys->clk + invalidate_time + aggregated_latency);
      }
    }

    lines->erase(victim);
  }

  std::list<Cache::Line>::iterator 
  Cache::allocate_line(std::list<Line>& lines, uint64_t addr) 
  {
    // See if an eviction is needed
    if (need_eviction(lines, addr))
    {
      // Get victim.
      // The first one might still be locked due to reorder in MC
      auto victim = find_if(lines.begin(), lines.end(),
          [this](Line line) 
          {
            bool check = !line.lock;
            if (!is_first_level) 
            {
              for (auto hc : higher_caches) 
              {
                if(!check)
                  return check;
                check = check && hc->check_unlock(line.addr);
              }
            }
            return check;
          });
      if (victim == lines.end())
        return victim;  // doesn't exist a line that's already unlocked in each level

      assert(victim != lines.end());
      evict(&lines, victim);
    }

    // Allocate newline, with lock bit on and dirty bit off
    lines.push_back(Line(addr, get_tag(addr)));
    auto last_element = lines.end();
    --last_element;
    return last_element;
  }

  bool Cache::is_hit(std::list<Line>& lines, uint64_t addr,
                     std::list<Line>::iterator* pos_ptr) 
  {
    auto pos = find_if(lines.begin(), lines.end(),
        [addr, this](Line l){return (l.tag == get_tag(addr));});
    *pos_ptr = pos;
    if (pos == lines.end())
      return false;

    return !pos->lock;
  }

  bool Cache::all_sets_locked(const std::list<Line>& lines) 
  {
    if (lines.size() < assoc) return false;
    for (const auto& line : lines) 
      if (!line.lock) 
        return false;
    return true;
  }

  bool Cache::check_unlock(uint64_t addr) 
  {
    auto it = cache_lines.find(get_index(addr));
    if (it == cache_lines.end())
      return true;
    else 
    {
      auto& lines = it->second;
      auto line = std::find_if(lines.begin(), lines.end(),
          [addr, this](Line l){return (l.tag == get_tag(addr));});
      if (line == lines.end())
        return true;
      else 
      {
        bool check = !line->lock;
        if (!is_first_level) 
        {
          for (auto hc : higher_caches) 
          {
            if (!check)
              return check;
            check = check && hc->check_unlock(line->addr);
          }
        }
        return check;
      }
    }
  }

  std::vector<std::pair<uint64_t, std::list<Cache::Line>::iterator>>::iterator
  Cache::hit_mshr(uint64_t addr) 
  {
    auto mshr_it =
        std::find_if(mshr_entries.begin(), mshr_entries.end(),
            [addr, this](std::pair<uint64_t, std::list<Cache::Line>::iterator> mshr_entry) 
            {
              return (align(mshr_entry.first) == align(addr));
            });
    return mshr_it;
  }

  std::list<Cache::Line>& Cache::get_lines(uint64_t addr) 
  {
    if (cache_lines.find(get_index(addr)) == cache_lines.end()) 
      cache_lines.insert(make_pair(get_index(addr), std::list<Line>()));
    return cache_lines[get_index(addr)];
  }

  void Cache::concatlower(Cache* lower) {
    lower_cache = lower;
    assert(lower != nullptr);
    lower->higher_caches.push_back(this);
  };

  bool Cache::need_eviction(const std::list<Line>& lines, uint64_t addr) 
  {
    if (find_if(lines.begin(), lines.end(), 
              [addr, this](Line l) { return (get_tag(addr) == l.tag); }) 
        != lines.end()) 
    {
      // Due to MSHR, the program can't reach here. Just for checking
      assert(false);
      return false;
    } 
    else 
    {
      if (lines.size() < assoc)
        return false;
      else
        return true;
    }
  }

  void Cache::callback(Request& req) 
  {
    debug("level %d", int(level));

    auto it = find_if(mshr_entries.begin(), mshr_entries.end(),
        [&req, this](std::pair<uint64_t, std::list<Line>::iterator> mshr_entry) 
        { return (align(mshr_entry.first) == align(req.addr)); });

    if (it != mshr_entries.end()) 
    {
      it->second->lock = false;
      mshr_entries.erase(it);
    }

    if (higher_caches.size()) 
    {
      for (auto hc : higher_caches)
        hc->callback(req);
    }
  }

  void Cache::tick() 
  {

      if(!lower_cache->is_last_level)
          lower_cache->tick();

      for (auto it = retry_list.begin(); it != retry_list.end(); it++) 
      {
          if(lower_cache->send(*it))
              it = retry_list.erase(it);
      }
  }

  void CacheSystem::tick() 
  {
    debug("clk %ld", clk);

    ++clk;

    // Sends ready waiting request to memory
    auto it = wait_list.begin();
    while (it != wait_list.end() && clk >= it->first) 
    {
      if (!send_memory(it->second)) 
        ++it;
      else 
      {
        debug("complete req: addr %lx", (it->second).addr);
        it = wait_list.erase(it);
      }
    }

    // hit request callback
    it = hit_list.begin();
    while (it != hit_list.end()) 
    {
      if (clk >= it->first) 
      {
        it->second.callback(it->second);

        debug("finish hit: addr %lx", (it->second).addr);

        it = hit_list.erase(it);
      } 
      else
        ++it;
    }
  }

  CacheSystem::CacheSystem(const YAML::Node& config, std::function<bool(Request)> send_memory) :
  send_memory(send_memory), config(config)
  {
    // Resolve the cache hierarchy
    bool has_l1 = false;
    bool has_l2 = false;
    bool has_l3 = false;
    if (config["L1"])
      has_l1 = true;
    if (config["L2"])
      has_l2 = true;
    if (config["L3"])
      has_l3 = true;

    first_level = Cache::Level::MAX;
    last_level = Cache::Level::MAX;

    if (has_l1) first_level = Cache::Level::L1;
    if (has_l3) last_level = Cache::Level::L3;

    if (has_l1 && has_l2 && !has_l3) last_level = Cache::Level::L2;
    if (has_l2 && has_l3 && !has_l1) first_level = Cache::Level::L2;

    if (has_l1 && has_l3)
      if (!has_l2)
        assert(false && "If has L1 and L3 cache, must have L2 as well.");

    if (has_l1 && !has_l2 && !has_l3)
      last_level = Cache::Level::L1;

    if (!has_l1 && !has_l2 && has_l3)
      first_level = Cache::Level::L3;

    no_core_caches = (first_level == Cache::Level::L3) || (first_level == Cache::Level::MAX);
    no_shared_cache = (last_level == Cache::Level::L2 || last_level == Cache::Level::L1) ||
                      (last_level == Cache::Level::MAX);

    // Parse the cache configurations
    if (has_l1)
    {
      capacity_each[uint(Cache::Level::L1)] = Config::parse_capacity(config["L1"]["capacity"].as<std::string>("32KB"));
      assoc_each[uint(Cache::Level::L1)]    = config["L1"]["associativity"].as<uint>(8);
      mshr_num_each[uint(Cache::Level::L1)] = config["L1"]["mshr"].as<uint>(16);
      latency_each[uint(Cache::Level::L1)]  = config["L1"]["latency"].as<uint>(4);      
    }
    else
      latency_each[uint(Cache::Level::L1)] = 4;

    if (has_l2)
    {
      capacity_each[uint(Cache::Level::L2)] = Config::parse_capacity(config["L2"]["capacity"].as<std::string>("256KB"));
      assoc_each[uint(Cache::Level::L2)]    = config["L2"]["associativity"].as<uint>(8);
      mshr_num_each[uint(Cache::Level::L2)] = config["L2"]["mshr"].as<uint>(16);
      latency_each[uint(Cache::Level::L2)]  = config["L2"]["latency"].as<uint>(12);      
    }
    else
      latency_each[uint(Cache::Level::L2)] = 12;

    if (has_l3)
    {
      capacity_each[uint(Cache::Level::L3)] = Config::parse_capacity(config["L3"]["capacity"].as<std::string>("8MB"));
      assoc_each[uint(Cache::Level::L3)]    = config["L3"]["associativity"].as<uint>(8);
      mshr_num_each[uint(Cache::Level::L3)] = config["L3"]["mshr"].as<uint>(16);
      latency_each[uint(Cache::Level::L3)]  = config["L3"]["latency"].as<uint>(31);      
    }
    else
      latency_each[uint(Cache::Level::L3)] = 31;

  };
} // namespace Ramulator

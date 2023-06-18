#include "Translation.h"

namespace Ramulator
{
  TranslationBase* make_translation(const YAML::Node& config, uint64_t max_address)
  {
    if (! config["translation"])
      throw std::runtime_error("Address Translation configuration (translation:) not found!");

    const YAML::Node& translation_config = config["translation"];
    std::string translation_type = translation_config["type"].as<std::string>("");

    TranslationBase* translation = nullptr;

    if      (translation_type == "None")
      translation = new NoTranslation(translation_config, max_address);
    else if (translation_type == "Random")
      translation = new RandomTranslation(translation_config, max_address);
    else if (translation_type == "LessRandom")
      translation = new LessRandomTranslation(translation_config, max_address);
    else
      throw std::runtime_error(fmt::format("Unrecognized Address Mapper type: {}", translation_type));

    // std::cout << fmt::format("Address Mapper: {}", addr_mapper->to_string()) << std::endl;
    return translation;
  };


  TranslationBase::TranslationBase(const YAML::Node& config, uint64_t max_address)
  {
    free_physical_pages_remaining = max_address >> 12;
    free_physical_pages.resize(free_physical_pages_remaining, -1);

    physical_page_replacement
        .name("physical_page_replacement")
        .desc("The number of times that physical page replacement happens.")
        .precision(0)
        ;
  };


  RandomTranslation::RandomTranslation(const YAML::Node& config, uint64_t max_address):
  TranslationBase(config, max_address)
  {
    allocator_rng.seed(0);
  };

  uint64_t RandomTranslation::page_allocator(uint64_t addr, int coreid)
  {
    uint64_t virtual_page_number = addr >> 12;

    auto target = std::make_pair(coreid, virtual_page_number);
    if(page_translation.find(target) == page_translation.end()) 
    {
      // page doesn't exist, so assign a new page
      // make sure there are physical pages left to be assigned

      // if physical page doesn't remain, replace a previous assigned
      // physical page.
      if (!free_physical_pages_remaining) 
      {
        this->physical_page_replacement++;
        uint64_t phys_page_to_read = allocator_rng() % free_physical_pages.size();
        assert(free_physical_pages[phys_page_to_read] != -1);
        page_translation[target] = phys_page_to_read;
      } 
      else 
      {
        // assign a new page
        uint64_t phys_page_to_read = allocator_rng() % free_physical_pages.size();
        // if the randomly-selected page was already assigned
        if(free_physical_pages[phys_page_to_read] != -1) 
        {
          uint64_t starting_page_of_search = phys_page_to_read;

          do 
          {
            // iterate through the list until we find a free page
            // TODO: does this introduce serious non-randomness?
            ++phys_page_to_read;
            phys_page_to_read %= free_physical_pages.size();
          }
          while((phys_page_to_read != starting_page_of_search) && free_physical_pages[phys_page_to_read] != -1);
        }

        assert(free_physical_pages[phys_page_to_read] == -1);

        page_translation[target] = phys_page_to_read;
        free_physical_pages[phys_page_to_read] = coreid;
        --free_physical_pages_remaining;
        }
    }

    // SAUGATA TODO: page size should not always be fixed to 4KB
    return (page_translation[target] << 12) | (addr & ((1 << 12) - 1));
  };

  LessRandomTranslation::LessRandomTranslation(const YAML::Node& config, uint64_t max_address):
  TranslationBase(config, max_address)
  {
    allocator_rng.seed(0);
    // assign 128 pages (512 KiB) contiguously before moving to the next 
    // random contiguously allocated memory block 
    contiguous_allocation_quota = 128;
    current_contiguously_allocated_pages = 128;
    current_base = 0;
  };

  uint64_t LessRandomTranslation::page_allocator(uint64_t addr, int coreid)
  {
    uint64_t virtual_page_number = addr >> 12;

    auto target = std::make_pair(coreid, virtual_page_number);
    if(page_translation.find(target) == page_translation.end()) 
    {
      // page doesn't exist, so assign a new page
      // make sure there are physical pages left to be assigned
      assert(free_physical_pages_remaining && "Physical page allocation quota not met!");
      
      if (current_contiguously_allocated_pages == contiguous_allocation_quota)
      {
        // assign a new page
        uint64_t phys_page_to_read = (allocator_rng() % (free_physical_pages.size() / contiguous_allocation_quota)) * contiguous_allocation_quota;
        // if the randomly-selected page was already assigned
        if(free_physical_pages[phys_page_to_read] != -1) 
        {
          uint64_t starting_page_of_search = phys_page_to_read;

          do 
          {
            // iterate through the list until we find a free page
            // TODO: does this introduce serious non-randomness?
            phys_page_to_read += contiguous_allocation_quota;
            phys_page_to_read %= free_physical_pages.size();
          }
          while((phys_page_to_read != starting_page_of_search) && free_physical_pages[phys_page_to_read] != -1);
        }

        assert(free_physical_pages[phys_page_to_read] == -1);

        current_base = phys_page_to_read;

        page_translation[target] = phys_page_to_read;
        free_physical_pages[phys_page_to_read] = coreid;
        --free_physical_pages_remaining;

        current_contiguously_allocated_pages = 1;
      }
      else
      {
        uint64_t phys_page_to_read = current_base + current_contiguously_allocated_pages;
        assert(free_physical_pages[phys_page_to_read] == -1);
        page_translation[target] = phys_page_to_read;
        free_physical_pages[phys_page_to_read] = coreid;
        ++current_contiguously_allocated_pages;
        --free_physical_pages_remaining;
      }
    }

    // SAUGATA TODO: page size should not always be fixed to 4KB
    return (page_translation[target] << 12) | (addr & ((1 << 12) - 1));
  };  
};
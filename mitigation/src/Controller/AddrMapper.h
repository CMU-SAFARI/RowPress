#pragma once

#include <vector>

#include "Config.h"
#include "Request.h"

namespace Ramulator
{
  template <typename T>
  class Controller;


  template <typename T>
  class AddrMapperBase
  {
    public:
      Controller<T>* ctrl;
      std::vector<int> addr_bits;       ///< How many address bits are there for each level in the hierarchy?


    public:
      AddrMapperBase(const YAML::Node& config, Controller<T>* ctrl);
      virtual ~AddrMapperBase() = default;
      virtual std::string to_string() = 0;

    public:
      /**
       * @brief    Apply the physical to DRAM address mapping.
       * 
       * @details
       *  Apply the address mapping to update the DRAM address vector (e.g., Rank, Bank Group, Bank addresses) of a request \p req
       *  according to the mapping scheme and the physical address \p addr.
       * 
       * @param    addr           The physical address.
       * @param    req            The request whose DRAM address vector will be updated.
       */
      virtual void apply(uint64_t addr, Request& req) = 0;
  };


  template <typename T>
  class LinearMapper : public AddrMapperBase<T>
  {
    public:
      enum class Type : int
      {
        ChRaBaRoCo,
        RoBaRaCoCh,
        MOP4CLXOR,
        MAX,
      } type = Type::MOP4CLXOR;


    public:
      LinearMapper(const YAML::Node& config, Controller<T>* ctrl) :
      AddrMapperBase<T>(config, ctrl)
      {
        std::string _type = config["mapping"].as<std::string>("MOP4CLXOR");
        if      (_type == "RoBaRaCoCh") 
          type = Type::RoBaRaCoCh;
        else if (_type == "ChRaBaRoCo") 
          type = Type::ChRaBaRoCo;
        else if (_type == "MOP4CLXOR")
          type = Type::MOP4CLXOR;
        else
          throw std::runtime_error(fmt::format("Unrecognized Linear Address Mapper type: {}", _type));
      }

    public:
      virtual void apply(uint64_t addr, Request& req) override;
      std::string to_string() override { return "Linear map."; };
  };
}

#include "AddrMapper.tpp"

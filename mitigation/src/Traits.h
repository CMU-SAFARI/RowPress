#pragma once

#include <type_traits>

#include "Standards.h"

namespace Ramulator
{
  namespace Traits
  {
    // Refresh
    template <class T>
    inline constexpr bool has_RH_defense = std::false_type::value;
    template <>
    inline constexpr bool has_RH_defense<DDR5> = std::true_type::value;

    // Printing command traces
    template <class T>
    inline constexpr bool has_BankGroup = std::false_type::value;
    template <>
    inline constexpr bool has_BankGroup<DDR4> = std::true_type::value;
    template <>
    inline constexpr bool has_BankGroup<DDR5> = std::true_type::value;

    #if defined(RAMULATOR_POWER)
    // Power model available
    template <class T>
    inline constexpr bool has_PowerModel = std::false_type::value;
    template <>
    inline constexpr bool has_PowerModel<DDR4> = std::true_type::value;
    #endif
  }
}
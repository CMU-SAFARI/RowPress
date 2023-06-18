#pragma once
#include <cstdint>

int calc_log2(int val);
int slice_lower_bits(uint64_t& addr, int bits);
void clear_lower_bits(uint64_t& addr, int bits);
bool get_bit_at(uint64_t addr, int bit);

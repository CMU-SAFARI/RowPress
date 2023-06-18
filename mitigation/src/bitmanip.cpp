#include "bitmanip.h"

int calc_log2(int val)
{
  int n = 0;
  while ((val >>= 1))
    n ++;
  return n;
}

int slice_lower_bits(uint64_t& addr, int bits)
{
  int lbits = addr & ((1<<bits) - 1);
  addr >>= bits;
  return lbits;
}

bool get_bit_at(uint64_t addr, int bit)
{
  return (((addr >> bit) & 1) == 1);
}

void clear_lower_bits(uint64_t& addr, int bits)
{
  addr >>= bits;
}
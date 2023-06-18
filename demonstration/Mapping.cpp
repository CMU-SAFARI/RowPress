#include "Mapping.h"

uintptr_t Mapping::base_address;

void Mapping::decode_new_address(uintptr_t addr)
{
    auto p = addr;
    int res = 0;
    for (unsigned long i : dual_rank.DRAM_MTX) {
        res <<= 1ULL;
        res |= (int) __builtin_parityl(p & i);
    }
    bank = (res >> dual_rank.BK_SHIFT) & dual_rank.BK_MASK;
    row = (res >> dual_rank.ROW_SHIFT) & dual_rank.ROW_MASK;
    column = (res >> dual_rank.COL_SHIFT) & dual_rank.COL_MASK;
}

int Mapping::linearize() {
  return (this->bank << dual_rank.BK_SHIFT) 
        | (this->row << dual_rank.ROW_SHIFT) 
        | (this->column << dual_rank.COL_SHIFT);
}

uintptr_t Mapping::to_virt() {
  int res = 0;
  int l = this->linearize();
  for (unsigned long i : dual_rank.ADDR_MTX) {
    res <<= 1ULL;
    res |= (int) __builtin_parityl(l & i);
  }
  return res + this->base_address;
}

void Mapping::increment_row()
{
  this->row++;
}

void Mapping::increment_bank()
{
  this->bank++;
}

void Mapping::decrement_row()
{
  this->row--;
}

void Mapping::increment_column_dw()
{
  this->column+=8;
}

void Mapping::increment_column_cb()
{
  this->column+=64;
}

void Mapping::reset_column()
{
  this->column = 0;
}

int Mapping::get_bank()
{
    return bank;
}

int Mapping::get_row()
{
    return row;
}

int Mapping::get_column()
{
    return column;
}
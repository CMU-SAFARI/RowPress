#include "Tester.h"

namespace RowPress
{
  void Tester::SMC_initialize_row(const CacheLine_t& cacheline, uint bank, uint row)
  {
    Program p;
    // Load bank and row address registers
    p.add_inst(SMC_LI(bank, BAR));
    p.add_inst(SMC_LI(row, RAR));
    // Column address stride is 8 since we are doing BL=8
    p.add_inst(SMC_LI(8, CASR));

    // Load the cache line into the wide data register
    for(uint i = 0 ; i < 16 ; i++)
    {
      p.add_inst(SMC_LI(cacheline[i], PATTERN_REG));
      p.add_inst(SMC_LDWD(PATTERN_REG, i));
    }

    // Activate and write to the row
    p.add_inst(SMC_PRE(BAR, 0, 1), SMC_NOP(), SMC_NOP(), SMC_NOP());
    p.add_inst(SMC_LI(0, CAR));
    p.add_inst(all_nops()); 
    p.add_inst(all_nops()); 

    p.add_inst(SMC_ACT(BAR, 0, RAR, 0), SMC_NOP(), SMC_NOP(), SMC_NOP());
    p.add_inst(all_nops()); 
    p.add_inst(all_nops());

    for(int i = 0 ; i < 128 ; i++)
    {
      p.add_inst(SMC_WRITE(BAR, 0, CAR, 1, 0, 0), SMC_NOP(), SMC_NOP(), SMC_NOP());
      p.add_inst(all_nops()); 
    }
    p.add_inst(SMC_SLEEP(4));
    
    p.add_inst(SMC_PRE(BAR, 0, 0), SMC_NOP(), SMC_NOP(), SMC_NOP());
    p.add_inst(all_nops()); 
    p.add_inst(all_nops());

    p.add_inst(SMC_END());
    
    platform.execute(p);    
  }

  void Tester::SMC_initialize_rows(const CacheLine_t& cacheline, uint bank, std::vector<uint>& rows)
  {
    Program p;

    uint num_rows = rows.size();
    uint num_rows_reg = 7;
    uint row_ptr_reg = 8;
    uint row_reg = 9;
   
    // Store the row addresses into SoftMC memory
    p.add_inst(SMC_LI(num_rows, num_rows_reg));
    p.add_inst(SMC_LI(0, row_ptr_reg));

    for (uint i = 0; i < num_rows; i++)
    {
      p.add_inst(SMC_LI((uint32_t) rows[i], row_reg));
      p.add_inst(SMC_ST(row_ptr_reg, i, row_reg));
    }

    // Load bank and row address registers
    p.add_inst(SMC_LI(bank, BAR));
    // Column address stride is 8 since we are doing BL=8
    p.add_inst(SMC_LI(8, CASR));

    p.add_inst(SMC_PRE(BAR, 0, 1), SMC_NOP(), SMC_NOP(), SMC_NOP());
    p.add_inst(SMC_LI(0, CAR));
    p.add_inst(all_nops()); 
    p.add_inst(all_nops()); 

    // Loop through all stored row addresses
    p.add_label("WR_ROW_BEGIN");
    p.add_inst(SMC_LD(row_ptr_reg, 0, RAR));
    p.add_inst(SMC_ADDI(row_ptr_reg, 1, row_ptr_reg));

    p.add_inst(SMC_ACT(BAR, 0, RAR, 0), SMC_NOP(), SMC_NOP(), SMC_NOP());
    p.add_inst(SMC_LI(0, CAR));
    p.add_inst(all_nops());
    for(int i = 0 ; i < 128 ; i++)
    {
      p.add_inst(SMC_WRITE(BAR, 0, CAR, 1, 0, 0), SMC_NOP(), SMC_NOP(), SMC_NOP());
      p.add_inst(all_nops()); 
    }
    p.add_inst(SMC_SLEEP(4));
    
    p.add_inst(SMC_PRE(BAR, 0, 0), SMC_NOP(), SMC_NOP(), SMC_NOP());

    p.add_branch(p.BR_TYPE::BL, row_ptr_reg, num_rows_reg, "WR_ROW_BEGIN");

    p.add_inst(SMC_END());
    
    platform.execute(p);
  }

  void Tester::SMC_read_row(uint bank, uint row)
  {
    Program p;
    // Load bank and row address registers
    p.add_inst(SMC_LI(bank, BAR));
    p.add_inst(SMC_LI(row, RAR));
    // Column address stride is 8 since we are doing BL=8
    p.add_inst(SMC_LI(8, CASR));

    // Activate and read from the row
    p.add_inst(SMC_PRE(BAR, 0, 1), SMC_NOP(), SMC_NOP(), SMC_NOP());
    p.add_inst(SMC_LI(0, CAR));
    p.add_inst(all_nops());
    p.add_inst(all_nops());

    p.add_inst(SMC_ACT(BAR, 0, RAR, 0), SMC_NOP(), SMC_NOP(), SMC_NOP());
    p.add_inst(all_nops()); 
    p.add_inst(all_nops());

    for(int i = 0 ; i < 128 ; i++)
    {
      p.add_inst(SMC_READ(BAR, 0, CAR, 1, 0, 0), SMC_NOP(), SMC_NOP(), SMC_NOP());
      p.add_inst(all_nops()); 
    }
    p.add_inst(SMC_SLEEP(4));

    p.add_inst(SMC_PRE(BAR, 0, 0), SMC_NOP(), SMC_NOP(), SMC_NOP());
    p.add_inst(SMC_SLEEP(3));

    p.add_inst(SMC_END());

    platform.execute(p);
  };

  void Tester::SMC_read_rows(uint bank, const std::vector<Row_t>& victim_rows)
  {
    Program p;
    uint num_rows = victim_rows.size();
    uint num_rows_reg = 7;
    uint row_ptr_reg = 8;
    uint row_reg = 9;

    p.add_inst(SMC_LI(num_rows, num_rows_reg));
    p.add_inst(SMC_LI(0, row_ptr_reg));
    for (uint i = 0; i < num_rows; i++)
    {
      p.add_inst(SMC_LI((uint32_t) victim_rows[i].row_address, row_reg));
      p.add_inst(SMC_ST(row_ptr_reg, i, row_reg));
    }

    p.add_inst(SMC_LI(bank, BAR));
    p.add_inst(SMC_LI(8, CASR));

    p.add_inst(SMC_PRE(BAR, 0, 1), SMC_NOP(), SMC_NOP(), SMC_NOP());
    p.add_inst(all_nops());
    p.add_inst(all_nops());

    p.add_label("RD_ROW_BEGIN");
    p.add_inst(SMC_LD(row_ptr_reg, 0, RAR));
    p.add_inst(SMC_ADDI(row_ptr_reg, 1, row_ptr_reg));

    p.add_inst(SMC_ACT(BAR, 0, RAR, 0), SMC_NOP(), SMC_NOP(), SMC_NOP());
    p.add_inst(SMC_LI(0, CAR));
    p.add_inst(all_nops());

    for(int i = 0 ; i < 128 ; i++)
    {
      p.add_inst(SMC_READ(BAR, 0, CAR, 1, 0, 0), SMC_NOP(), SMC_NOP(), SMC_NOP());
      p.add_inst(all_nops()); 
    }
    p.add_inst(SMC_SLEEP(4));

    p.add_inst(SMC_PRE(BAR, 0, 0), SMC_NOP(), SMC_NOP(), SMC_NOP());
    p.add_branch(p.BR_TYPE::BL, row_ptr_reg, num_rows_reg, "RD_ROW_BEGIN");

    p.add_inst(SMC_END());

    platform.execute(p);
  };

  void Tester::SMC_act_rows(uint bank, const std::vector<Row_t>& victim_rows)
  {
    Program p;
    uint num_rows = victim_rows.size();
    uint num_rows_reg = 7;
    uint row_ptr_reg = 8;
    uint row_reg = 9;

    p.add_inst(SMC_LI(num_rows, num_rows_reg));
    p.add_inst(SMC_LI(0, row_ptr_reg));
    for (uint i = 0; i < num_rows; i++)
    {
      p.add_inst(SMC_LI((uint32_t) victim_rows[i].row_address, row_reg));
      p.add_inst(SMC_ST(row_ptr_reg, i, row_reg));
    }

    p.add_inst(SMC_LI(bank, BAR));
    p.add_inst(SMC_LI(8, CASR));

    p.add_inst(SMC_PRE(BAR, 0, 1), SMC_NOP(), SMC_NOP(), SMC_NOP());
    p.add_inst(all_nops());
    p.add_inst(all_nops());

    p.add_label("RD_ROW_BEGIN");
    p.add_inst(SMC_LD(row_ptr_reg, 0, RAR));
    p.add_inst(SMC_ADDI(row_ptr_reg, 1, row_ptr_reg));

    p.add_inst(SMC_ACT(BAR, 0, RAR, 0), SMC_NOP(), SMC_NOP(), SMC_NOP());
    p.add_inst(SMC_LI(0, CAR));
    p.add_inst(SMC_SLEEP(5));

    p.add_inst(SMC_PRE(BAR, 0, 0), SMC_NOP(), SMC_NOP(), SMC_NOP());
    p.add_branch(p.BR_TYPE::BL, row_ptr_reg, num_rows_reg, "RD_ROW_BEGIN");

    p.add_inst(SMC_END());

    platform.execute(p);
  };

  void Tester::SMC_hammer_row(uint bank, uint aggressor_row, uint hammer_count, uint RAS_scale, uint RP_scale)
  {
    Program p;

    // IMPORTANT: PREA to get around the auto-read of DRAM Bender
    p.add_inst(SMC_PRE(BAR, 0, 1), SMC_NOP(), SMC_NOP(), SMC_NOP());

    // Load bank and row address registers
    p.add_inst(SMC_LI(bank, BAR));
    p.add_inst(SMC_LI(aggressor_row, RAR));

    uint HMR_COUNTER_REG = 7;
    uint NUM_HMR_REG = 8;
    
    p.add_inst(SMC_LI(0, HMR_COUNTER_REG));
    p.add_inst(SMC_LI(hammer_count, NUM_HMR_REG));

    // Activate the aggressor row for hammer_count times
    p.add_label("HMR_BEGIN");

    // Additive RAS latency after 36ns standard tRAS, step size is 5 * 6 = 30ns
    if (RAS_scale > 1)
      p.add_inst(SMC_SLEEP(5 * (RAS_scale - 1)));

    p.add_inst(SMC_PRE(BAR, 0, 0), SMC_NOP(), SMC_NOP(), SMC_NOP());
    p.add_inst(SMC_ADDI(HMR_COUNTER_REG, 1, HMR_COUNTER_REG));
    // Additive RP latency after 15ns standard tRP, step size is 5 * 6 = 30ns
    if (RP_scale > 1)
      p.add_inst(SMC_SLEEP(5 * (RP_scale - 1)));

    p.add_inst(SMC_NOP(), SMC_NOP(), SMC_NOP(), SMC_ACT(BAR, 0, RAR, 0));
    p.add_branch(p.BR_TYPE::BL, HMR_COUNTER_REG, NUM_HMR_REG, "HMR_BEGIN");

    p.add_inst(SMC_PRE(BAR, 0, 0), SMC_NOP(), SMC_NOP(), SMC_NOP());

    p.add_inst(all_nops()); 
    p.add_inst(SMC_END());

    platform.execute(p);
  };

  void Tester::SMC_hammer_row_double(uint bank, uint aggressor_row_1, uint aggressor_row_2, uint hammer_count, uint RAS_scale, uint RP_scale)
  {
    uint HMR_COUNTER_REG = 7;
    uint NUM_HMR_REG = 8;

    Program p;

    // IMPORTANT: PREA to get around the auto-read of DRAM Bender
    p.add_inst(SMC_PRE(BAR, 0, 1), SMC_NOP(), SMC_NOP(), SMC_NOP());

    // Load bank and row address registers
    p.add_inst(SMC_LI(bank, BAR));
    p.add_inst(SMC_LI(aggressor_row_1, RAR));

    p.add_inst(SMC_LI(0, HMR_COUNTER_REG));
    p.add_inst(SMC_LI(hammer_count, NUM_HMR_REG));

    // Activate the aggressor row for hammer_count times
    p.add_label("HMR_BEGIN");
    /////////////////////////////////////////////////////////////////////
    // Additive RAS latency after 36ns standard tRAS, step size is 5 * 6 = 30ns
    if (RAS_scale > 1)
      p.add_inst(SMC_SLEEP(5 * (RAS_scale - 1)));

    // PRE aggressor row 2
    p.add_inst(SMC_PRE(BAR, 0, 0), SMC_NOP(), SMC_NOP(), SMC_NOP());
    p.add_inst(SMC_LI(aggressor_row_1, RAR));
    // Additive RP latency after 15ns standard tRP, step size is 5 * 6 = 30ns
    if (RP_scale > 1)
      p.add_inst(SMC_SLEEP(5 * (RP_scale - 1)));
    /////////////////////////////////////////////////////////////////////


    /////////////////////////////////////////////////////////////////////
    // ACT aggressor row 1
    p.add_inst(SMC_NOP(), SMC_NOP(), SMC_NOP(), SMC_ACT(BAR, 0, RAR, 0));
    p.add_inst(SMC_LI(aggressor_row_2, RAR));
    p.add_inst(SMC_SLEEP(5));
    // Additive RAS latency after 36ns standard tRAS, step size is 5 * 6 = 30ns
    if (RAS_scale > 1)
      p.add_inst(SMC_SLEEP(5 * (RAS_scale - 1)));

    // PRE aggressor row 1
    p.add_inst(SMC_PRE(BAR, 0, 0), SMC_NOP(), SMC_NOP(), SMC_NOP());
    p.add_inst(SMC_ADDI(HMR_COUNTER_REG, 1, HMR_COUNTER_REG));
    // Additive RP latency after 15ns standard tRP, step size is 5 * 6 = 30ns
    if (RP_scale > 1)
      p.add_inst(SMC_SLEEP(5 * (RP_scale - 1)));
    /////////////////////////////////////////////////////////////////////


    /////////////////////////////////////////////////////////////////////
    // ACT aggressor row 2
    p.add_inst(SMC_NOP(), SMC_NOP(), SMC_NOP(), SMC_ACT(BAR, 0, RAR, 0));
    p.add_branch(p.BR_TYPE::BL, HMR_COUNTER_REG, NUM_HMR_REG, "HMR_BEGIN");
    /////////////////////////////////////////////////////////////////////

    p.add_inst(SMC_PRE(BAR, 0, 0), SMC_NOP(), SMC_NOP(), SMC_NOP());

    p.add_inst(all_nops()); 
    p.add_inst(SMC_END());

    platform.execute(p);
  };

  void Tester::SMC_hammer_row_and_read_victim(uint bank, uint aggressor_row, uint hammer_count, uint RAS_scale, uint RP_scale, uint victim_row)
  {
    Program p;

    // IMPORTANT: PREA to get around the auto-read of DRAM Bender
    p.add_inst(SMC_PRE(BAR, 0, 1), SMC_NOP(), SMC_NOP(), SMC_NOP());

    // Load bank and row address registers
    p.add_inst(SMC_LI(bank, BAR));
    p.add_inst(SMC_LI(aggressor_row, RAR));

    uint HMR_COUNTER_REG = 7;
    uint NUM_HMR_REG = 8;
    
    p.add_inst(SMC_LI(0, HMR_COUNTER_REG));
    p.add_inst(SMC_LI(hammer_count, NUM_HMR_REG));

    // Activate the aggressor row for hammer_count times
    p.add_label("HMR_BEGIN");

    // Additive RAS latency after 36ns standard tRAS, step size is 5 * 6 = 30ns
    if (RAS_scale > 1)
      p.add_inst(SMC_SLEEP(5 * (RAS_scale - 1)));

    p.add_inst(SMC_PRE(BAR, 0, 0), SMC_NOP(), SMC_NOP(), SMC_NOP());
    p.add_inst(SMC_ADDI(HMR_COUNTER_REG, 1, HMR_COUNTER_REG));
    // Additive RP latency after 15ns standard tRP, step size is 5 * 6 = 30ns
    if (RP_scale > 1)
      p.add_inst(SMC_SLEEP(5 * (RP_scale - 1)));

    p.add_inst(SMC_NOP(), SMC_NOP(), SMC_NOP(), SMC_ACT(BAR, 0, RAR, 0));
    p.add_branch(p.BR_TYPE::BL, HMR_COUNTER_REG, NUM_HMR_REG, "HMR_BEGIN");

    p.add_inst(SMC_LI(victim_row, RAR));
    // Column address stride is 8 since we are doing BL=8
    p.add_inst(SMC_LI(8, CASR));

    // Activate and read from the row
    p.add_inst(SMC_PRE(BAR, 0, 0), SMC_NOP(), SMC_NOP(), SMC_NOP());
    p.add_inst(SMC_LI(0, CAR));
    p.add_inst(all_nops());

    p.add_inst(SMC_ACT(BAR, 0, RAR, 0), SMC_NOP(), SMC_NOP(), SMC_NOP());
    p.add_inst(all_nops()); 
    p.add_inst(all_nops());

    for(int i = 0 ; i < 128 ; i++)
    {
      p.add_inst(SMC_READ(BAR, 0, CAR, 1, 0, 0), SMC_NOP(), SMC_NOP(), SMC_NOP());
      p.add_inst(all_nops()); 
    }
    p.add_inst(SMC_SLEEP(4));


    p.add_inst(SMC_PRE(BAR, 0, 0), SMC_NOP(), SMC_NOP(), SMC_NOP());
    p.add_inst(SMC_SLEEP(3));

    p.add_inst(SMC_END());

    platform.execute(p);
  };

  void Tester::SMC_hammer_row_double_and_read_victim(uint bank, uint aggressor_row_1, uint aggressor_row_2, uint hammer_count, uint RAS_scale, uint RP_scale, uint victim_row)
  {
    uint HMR_COUNTER_REG = 7;
    uint NUM_HMR_REG = 8;

    Program p;

    // IMPORTANT: PREA to get around the auto-read of DRAM Bender
    p.add_inst(SMC_PRE(BAR, 0, 1), SMC_NOP(), SMC_NOP(), SMC_NOP());


    // Load bank and row address registers
    p.add_inst(SMC_LI(bank, BAR));
    p.add_inst(SMC_LI(aggressor_row_1, RAR));

    p.add_inst(SMC_LI(0, HMR_COUNTER_REG));
    p.add_inst(SMC_LI(hammer_count, NUM_HMR_REG));

    // Activate the aggressor row for hammer_count times
    p.add_label("HMR_BEGIN");
    /////////////////////////////////////////////////////////////////////
    // Additive RAS latency after 36ns standard tRAS, step size is 5 * 6 = 30ns
    if (RAS_scale > 1)
      p.add_inst(SMC_SLEEP(5 * (RAS_scale - 1)));

    // PRE aggressor row 2
    p.add_inst(SMC_PRE(BAR, 0, 0), SMC_NOP(), SMC_NOP(), SMC_NOP());
    p.add_inst(SMC_LI(aggressor_row_1, RAR));
    // Additive RP latency after 15ns standard tRP, step size is 5 * 6 = 30ns
    if (RP_scale > 1)
      p.add_inst(SMC_SLEEP(5 * (RP_scale - 1)));
    /////////////////////////////////////////////////////////////////////


    /////////////////////////////////////////////////////////////////////
    // ACT aggressor row 1
    p.add_inst(SMC_NOP(), SMC_NOP(), SMC_NOP(), SMC_ACT(BAR, 0, RAR, 0));
    p.add_inst(SMC_LI(aggressor_row_2, RAR));
    p.add_inst(SMC_SLEEP(5));
    // Additive RAS latency after 36ns standard tRAS, step size is 5 * 6 = 30ns
    if (RAS_scale > 1)
      p.add_inst(SMC_SLEEP(5 * (RAS_scale - 1)));

    // PRE aggressor row 1
    p.add_inst(SMC_PRE(BAR, 0, 0), SMC_NOP(), SMC_NOP(), SMC_NOP());
    p.add_inst(SMC_ADDI(HMR_COUNTER_REG, 1, HMR_COUNTER_REG));
    // Additive RP latency after 15ns standard tRP, step size is 5 * 6 = 30ns
    if (RP_scale > 1)
      p.add_inst(SMC_SLEEP(5 * (RP_scale - 1)));
    /////////////////////////////////////////////////////////////////////


    /////////////////////////////////////////////////////////////////////
    // ACT aggressor row 2
    p.add_inst(SMC_NOP(), SMC_NOP(), SMC_NOP(), SMC_ACT(BAR, 0, RAR, 0));
    p.add_branch(p.BR_TYPE::BL, HMR_COUNTER_REG, NUM_HMR_REG, "HMR_BEGIN");
    /////////////////////////////////////////////////////////////////////

    p.add_inst(SMC_LI(victim_row, RAR));
    // Column address stride is 8 since we are doing BL=8
    p.add_inst(SMC_LI(8, CASR));

    // Activate and read from the row
    p.add_inst(SMC_PRE(BAR, 0, 0), SMC_NOP(), SMC_NOP(), SMC_NOP());
    p.add_inst(SMC_LI(0, CAR));
    p.add_inst(all_nops());

    p.add_inst(SMC_ACT(BAR, 0, RAR, 0), SMC_NOP(), SMC_NOP(), SMC_NOP());
    p.add_inst(all_nops()); 
    p.add_inst(all_nops());

    for(int i = 0 ; i < 128 ; i++)
    {
      p.add_inst(SMC_READ(BAR, 0, CAR, 1, 0, 0), SMC_NOP(), SMC_NOP(), SMC_NOP());
      p.add_inst(all_nops()); 
    }
    p.add_inst(all_nops()); 
    p.add_inst(all_nops()); 

    p.add_inst(SMC_PRE(BAR, 0, 0), SMC_NOP(), SMC_NOP(), SMC_NOP());
    p.add_inst(SMC_SLEEP(3));

    p.add_inst(SMC_END());

    platform.execute(p);
  };


  void Tester::SMC_hammer_rows(uint bank, const std::vector<Row_t>& aggressor_rows, uint hammer_count, 
                               uint RAS_scale, uint RP_scale, bool is_retention)
  {
    Program p;

    uint HMR_COUNTER_REG = 7;
    uint NUM_HMR_REG = 8;

    // IMPORTANT: PREA to get around the auto-read of DRAM Bender
    p.add_inst(SMC_PRE(BAR, 0, 1), SMC_NOP(), SMC_NOP(), SMC_NOP());

    p.add_inst(SMC_LI(bank, BAR));
    p.add_inst(SMC_LI(aggressor_rows[0].row_address, RAR));

    p.add_inst(SMC_LI(1, HMR_COUNTER_REG));
    p.add_inst(SMC_LI(hammer_count, NUM_HMR_REG));

    if (is_retention)
      p.add_inst(all_nops());
    else
      p.add_inst(SMC_NOP(), SMC_NOP(), SMC_NOP(), SMC_ACT(BAR, 0, RAR, 0));

    p.add_branch(p.BR_TYPE::BEQ, HMR_COUNTER_REG, NUM_HMR_REG, "END");
    p.add_label("HMR_BEGIN");
    for (uint i = 1; i < aggressor_rows.size(); i++)
    {
      if (RAS_scale > 0)
        p.add_inst(SMC_SLEEP(5 * RAS_scale));

      if (is_retention)
        p.add_inst(all_nops());
      else
        p.add_inst(SMC_PRE(BAR, 0, 0), SMC_NOP(), SMC_NOP(), SMC_NOP());

      p.add_inst(SMC_LI(aggressor_rows[i].row_address, RAR));
      if (RP_scale > 0)
        p.add_inst(SMC_SLEEP(5 * RP_scale));
      
      if (is_retention)
        p.add_inst(all_nops());
      else      
        p.add_inst(SMC_NOP(), SMC_NOP(), SMC_NOP(), SMC_ACT(BAR, 0, RAR, 0));

      if (i == aggressor_rows.size() - 1)
        p.add_inst(SMC_SLEEP(5));
      else
        p.add_inst(SMC_SLEEP(6));
    }
    if (aggressor_rows.size() > 1)
      p.add_inst(SMC_ADDI(HMR_COUNTER_REG, 1, HMR_COUNTER_REG));
    if (RAS_scale > 0)
      p.add_inst(SMC_SLEEP(5 * RAS_scale));

    if (is_retention)
      p.add_inst(all_nops());
    else
      p.add_inst(SMC_PRE(BAR, 0, 0), SMC_NOP(), SMC_NOP(), SMC_NOP());

    if (aggressor_rows.size() > 1)
      p.add_inst(SMC_LI(aggressor_rows[0].row_address, RAR));
    else
      p.add_inst(SMC_ADDI(HMR_COUNTER_REG, 1, HMR_COUNTER_REG));

    if (RP_scale > 0)
      p.add_inst(SMC_SLEEP(5 * RP_scale));

    if (is_retention)
      p.add_inst(all_nops());
    else
      p.add_inst(SMC_NOP(), SMC_NOP(), SMC_NOP(), SMC_ACT(BAR, 0, RAR, 0));

    p.add_branch(p.BR_TYPE::BL, HMR_COUNTER_REG, NUM_HMR_REG, "HMR_BEGIN");
    p.add_label("END");

    if (RAS_scale > 0)
      p.add_inst(SMC_SLEEP(5 * RAS_scale));

    for (uint i = 1; i < aggressor_rows.size(); i++)
    {
      if (is_retention)
        p.add_inst(all_nops());
      else
        p.add_inst(SMC_PRE(BAR, 0, 0), SMC_NOP(), SMC_NOP(), SMC_NOP());

      p.add_inst(SMC_LI(aggressor_rows[i].row_address, RAR));
      if (RP_scale > 0)
        p.add_inst(SMC_SLEEP(5 * RP_scale));

      if (is_retention)
        p.add_inst(all_nops());
      else
        p.add_inst(SMC_NOP(), SMC_NOP(), SMC_NOP(), SMC_ACT(BAR, 0, RAR, 0));

      p.add_inst(SMC_SLEEP(6));
      if (RAS_scale > 0)
        p.add_inst(SMC_SLEEP(5 * RAS_scale));
    }

    if (is_retention)
      p.add_inst(all_nops());
    else
      p.add_inst(SMC_PRE(BAR, 0, 0), SMC_NOP(), SMC_NOP(), SMC_NOP());

    if (RP_scale > 0)
      p.add_inst(SMC_SLEEP(5 * RP_scale));

    p.add_inst(all_nops());
    p.add_inst(SMC_END());

    platform.execute(p);
  }


  void Tester::SMC_hammer_rows_RAS_ratio(uint bank, const std::vector<Row_t>& aggressor_rows, uint hammer_count, uint extra_cycles, float RAS_ratio, bool is_retention)
  {
    uint tAggOn = extra_cycles * RAS_ratio;
    uint tAggOff = extra_cycles - tAggOn;

    Program p;

    uint HMR_COUNTER_REG = 7;
    uint NUM_HMR_REG = 8;

    // IMPORTANT: PREA to get around the auto-read of DRAM Bender
    p.add_inst(SMC_PRE(BAR, 0, 1), SMC_NOP(), SMC_NOP(), SMC_NOP());

    p.add_inst(SMC_LI(bank, BAR));
    p.add_inst(SMC_LI(aggressor_rows[0].row_address, RAR));

    p.add_inst(SMC_LI(1, HMR_COUNTER_REG));
    p.add_inst(SMC_LI(hammer_count, NUM_HMR_REG));

    if (is_retention)
      p.add_inst(all_nops());
    else
      p.add_inst(SMC_NOP(), SMC_NOP(), SMC_NOP(), SMC_ACT(BAR, 0, RAR, 0));

    p.add_branch(p.BR_TYPE::BEQ, HMR_COUNTER_REG, NUM_HMR_REG, "END");
    p.add_label("HMR_BEGIN");
    for (uint i = 1; i < aggressor_rows.size(); i++)
    {
      if (tAggOn > 0)
      {
        if (tAggOn >= 3)
          p.add_inst(SMC_SLEEP(tAggOn));
        else
          for (uint i = 0; i < tAggOn; i++)
            p.add_inst(all_nops());
      }

      if (is_retention)
        p.add_inst(all_nops());
      else
        p.add_inst(SMC_PRE(BAR, 0, 0), SMC_NOP(), SMC_NOP(), SMC_NOP());

      p.add_inst(SMC_LI(aggressor_rows[i].row_address, RAR));

      if (tAggOff > 0)
      {
        if (tAggOff >= 3)
          p.add_inst(SMC_SLEEP(tAggOff));
        else
          for (uint i = 0; i < tAggOff; i++)
            p.add_inst(all_nops());
      }
      
      if (is_retention)
        p.add_inst(all_nops());
      else      
        p.add_inst(SMC_NOP(), SMC_NOP(), SMC_NOP(), SMC_ACT(BAR, 0, RAR, 0));

      if (i == aggressor_rows.size() - 1)
        p.add_inst(SMC_SLEEP(5));
      else
        p.add_inst(SMC_SLEEP(6));
    }
    if (aggressor_rows.size() > 1)
      p.add_inst(SMC_ADDI(HMR_COUNTER_REG, 1, HMR_COUNTER_REG));

    if (tAggOn > 0)
    {
      if (tAggOn >= 3)
        p.add_inst(SMC_SLEEP(tAggOn));
      else
        for (uint i = 0; i < tAggOn; i++)
          p.add_inst(all_nops());
    }

    if (is_retention)
      p.add_inst(all_nops());
    else
      p.add_inst(SMC_PRE(BAR, 0, 0), SMC_NOP(), SMC_NOP(), SMC_NOP());

    if (tAggOff > 0)
    {
      if (tAggOff >= 3)
        p.add_inst(SMC_SLEEP(tAggOff));
      else
        for (uint i = 0; i < tAggOff; i++)
          p.add_inst(all_nops());
    }

    if (aggressor_rows.size() > 1)
      p.add_inst(SMC_LI(aggressor_rows[0].row_address, RAR));
    else
      p.add_inst(SMC_ADDI(HMR_COUNTER_REG, 1, HMR_COUNTER_REG));

    if (is_retention)
      p.add_inst(all_nops());
    else
      p.add_inst(SMC_NOP(), SMC_NOP(), SMC_NOP(), SMC_ACT(BAR, 0, RAR, 0));

    p.add_branch(p.BR_TYPE::BL, HMR_COUNTER_REG, NUM_HMR_REG, "HMR_BEGIN");
    p.add_label("END");

    if (tAggOn > 0)
    {
      if (tAggOn >= 3)
        p.add_inst(SMC_SLEEP(tAggOn));
      else
        for (uint i = 0; i < tAggOn; i++)
          p.add_inst(all_nops());
    }

    for (uint i = 1; i < aggressor_rows.size(); i++)
    {
      if (is_retention)
        p.add_inst(all_nops());
      else
        p.add_inst(SMC_PRE(BAR, 0, 0), SMC_NOP(), SMC_NOP(), SMC_NOP());

      p.add_inst(SMC_LI(aggressor_rows[i].row_address, RAR));
      if (tAggOff > 0)
      {
        if (tAggOff >= 3)
          p.add_inst(SMC_SLEEP(tAggOff));
        else
          for (uint i = 0; i < tAggOff; i++)
            p.add_inst(all_nops());
      }

      if (is_retention)
        p.add_inst(all_nops());
      else
        p.add_inst(SMC_NOP(), SMC_NOP(), SMC_NOP(), SMC_ACT(BAR, 0, RAR, 0));

      p.add_inst(SMC_SLEEP(6));
      if (tAggOn > 0)
      {
        if (tAggOn >= 3)
          p.add_inst(SMC_SLEEP(tAggOn));
        else
          for (uint i = 0; i < tAggOn; i++)
            p.add_inst(all_nops());
      }
    }

    if (is_retention)
      p.add_inst(all_nops());
    else
      p.add_inst(SMC_PRE(BAR, 0, 0), SMC_NOP(), SMC_NOP(), SMC_NOP());

    if (tAggOff > 0)
    {
      if (tAggOff >= 3)
        p.add_inst(SMC_SLEEP(tAggOff));
      else
        for (uint i = 0; i < tAggOff; i++)
          p.add_inst(all_nops());
    }

    p.add_inst(all_nops());
    p.add_inst(SMC_END());

    platform.execute(p);
  }


  void Tester::SMC_asymmetric_rowpress(uint bank, const std::vector<Row_t>& aggressor_rows, uint head_RAS_scale, uint head_hammer_count, uint tail_RAS_scale, uint tail_hammer_count, bool is_retention)
  {
    Program p;

    uint HMR_COUNTER_REG = 7;
    uint NUM_HMR_REG = 8;

    // IMPORTANT: PREA to get around the auto-read of DRAM Bender
    p.add_inst(SMC_PRE(BAR, 0, 1), SMC_NOP(), SMC_NOP(), SMC_NOP());

    p.add_inst(SMC_LI(bank, BAR));
    p.add_inst(SMC_LI(aggressor_rows[0].row_address, RAR));

    p.add_inst(SMC_LI(1, HMR_COUNTER_REG));
    p.add_inst(SMC_LI(head_hammer_count, NUM_HMR_REG));

    if (is_retention)
      p.add_inst(all_nops());
    else
      p.add_inst(SMC_NOP(), SMC_NOP(), SMC_NOP(), SMC_ACT(BAR, 0, RAR, 0));

    p.add_branch(p.BR_TYPE::BEQ, HMR_COUNTER_REG, NUM_HMR_REG, "END");
    p.add_label("HMR_BEGIN");
    for (uint i = 1; i < aggressor_rows.size(); i++)
    {
      if (head_RAS_scale > 0)
        p.add_inst(SMC_SLEEP(5 * head_RAS_scale));

      if (is_retention)
        p.add_inst(all_nops());
      else
        p.add_inst(SMC_PRE(BAR, 0, 0), SMC_NOP(), SMC_NOP(), SMC_NOP());

      p.add_inst(SMC_LI(aggressor_rows[i].row_address, RAR));
      
      if (is_retention)
        p.add_inst(all_nops());
      else      
        p.add_inst(SMC_NOP(), SMC_NOP(), SMC_NOP(), SMC_ACT(BAR, 0, RAR, 0));

      if (i == aggressor_rows.size() - 1)
        p.add_inst(SMC_SLEEP(5));
      else
        p.add_inst(SMC_SLEEP(6));
    }
    if (aggressor_rows.size() > 1)
      p.add_inst(SMC_ADDI(HMR_COUNTER_REG, 1, HMR_COUNTER_REG));
    if (head_RAS_scale > 0)
      p.add_inst(SMC_SLEEP(5 * head_RAS_scale));

    if (is_retention)
      p.add_inst(all_nops());
    else
      p.add_inst(SMC_PRE(BAR, 0, 0), SMC_NOP(), SMC_NOP(), SMC_NOP());

    if (aggressor_rows.size() > 1)
      p.add_inst(SMC_LI(aggressor_rows[0].row_address, RAR));
    else
      p.add_inst(SMC_ADDI(HMR_COUNTER_REG, 1, HMR_COUNTER_REG));

    if (is_retention)
      p.add_inst(all_nops());
    else
      p.add_inst(SMC_NOP(), SMC_NOP(), SMC_NOP(), SMC_ACT(BAR, 0, RAR, 0));

    p.add_branch(p.BR_TYPE::BL, HMR_COUNTER_REG, NUM_HMR_REG, "HMR_BEGIN");
    p.add_label("END");

    if (head_RAS_scale > 0)
      p.add_inst(SMC_SLEEP(5 * head_RAS_scale));

    for (uint i = 1; i < aggressor_rows.size(); i++)
    {
      if (is_retention)
        p.add_inst(all_nops());
      else
        p.add_inst(SMC_PRE(BAR, 0, 0), SMC_NOP(), SMC_NOP(), SMC_NOP());

      p.add_inst(SMC_LI(aggressor_rows[i].row_address, RAR));

      if (is_retention)
        p.add_inst(all_nops());
      else
        p.add_inst(SMC_NOP(), SMC_NOP(), SMC_NOP(), SMC_ACT(BAR, 0, RAR, 0));

      p.add_inst(SMC_SLEEP(6));
      if (head_RAS_scale > 0)
        p.add_inst(SMC_SLEEP(5 * head_RAS_scale));
    }

    if (is_retention)
      p.add_inst(all_nops());
    else
      p.add_inst(SMC_PRE(BAR, 0, 0), SMC_NOP(), SMC_NOP(), SMC_NOP());


    //////////////////////////////////////////
    ////////////     TAIL     ////////////////
    //////////////////////////////////////////
    p.add_inst(SMC_LI(aggressor_rows[0].row_address, RAR));
    p.add_inst(SMC_LI(1, HMR_COUNTER_REG));
    p.add_inst(SMC_LI(tail_hammer_count, NUM_HMR_REG));

    if (is_retention)
      p.add_inst(all_nops());
    else
      p.add_inst(SMC_NOP(), SMC_NOP(), SMC_NOP(), SMC_ACT(BAR, 0, RAR, 0));

    p.add_branch(p.BR_TYPE::BEQ, HMR_COUNTER_REG, NUM_HMR_REG, "TAIL_END");
    p.add_label("TAIL_HMR_BEGIN");
    for (uint i = 1; i < aggressor_rows.size(); i++)
    {
      if (tail_RAS_scale > 0)
        p.add_inst(SMC_SLEEP(5 * tail_RAS_scale));

      if (is_retention)
        p.add_inst(all_nops());
      else
        p.add_inst(SMC_PRE(BAR, 0, 0), SMC_NOP(), SMC_NOP(), SMC_NOP());

      p.add_inst(SMC_LI(aggressor_rows[i].row_address, RAR));
      
      if (is_retention)
        p.add_inst(all_nops());
      else      
        p.add_inst(SMC_NOP(), SMC_NOP(), SMC_NOP(), SMC_ACT(BAR, 0, RAR, 0));

      if (i == aggressor_rows.size() - 1)
        p.add_inst(SMC_SLEEP(5));
      else
        p.add_inst(SMC_SLEEP(6));
    }
    if (aggressor_rows.size() > 1)
      p.add_inst(SMC_ADDI(HMR_COUNTER_REG, 1, HMR_COUNTER_REG));

    if (tail_RAS_scale > 0)
      p.add_inst(SMC_SLEEP(5 * tail_RAS_scale));

    if (is_retention)
      p.add_inst(all_nops());
    else
      p.add_inst(SMC_PRE(BAR, 0, 0), SMC_NOP(), SMC_NOP(), SMC_NOP());

    if (aggressor_rows.size() > 1)
      p.add_inst(SMC_LI(aggressor_rows[0].row_address, RAR));
    else
      p.add_inst(SMC_ADDI(HMR_COUNTER_REG, 1, HMR_COUNTER_REG));

    if (is_retention)
      p.add_inst(all_nops());
    else
      p.add_inst(SMC_NOP(), SMC_NOP(), SMC_NOP(), SMC_ACT(BAR, 0, RAR, 0));

    p.add_branch(p.BR_TYPE::BL, HMR_COUNTER_REG, NUM_HMR_REG, "TAIL_HMR_BEGIN");
    p.add_label("TAIL_END");

    if (tail_RAS_scale > 0)
      p.add_inst(SMC_SLEEP(5 * tail_RAS_scale));

    for (uint i = 1; i < aggressor_rows.size(); i++)
    {
      if (is_retention)
        p.add_inst(all_nops());
      else
        p.add_inst(SMC_PRE(BAR, 0, 0), SMC_NOP(), SMC_NOP(), SMC_NOP());

      p.add_inst(SMC_LI(aggressor_rows[i].row_address, RAR));

      if (is_retention)
        p.add_inst(all_nops());
      else
        p.add_inst(SMC_NOP(), SMC_NOP(), SMC_NOP(), SMC_ACT(BAR, 0, RAR, 0));

      p.add_inst(SMC_SLEEP(6));
      if (tail_RAS_scale > 0)
        p.add_inst(SMC_SLEEP(5 * tail_RAS_scale));
    }

    if (is_retention)
      p.add_inst(all_nops());
    else
      p.add_inst(SMC_PRE(BAR, 0, 0), SMC_NOP(), SMC_NOP(), SMC_NOP());

    p.add_inst(all_nops());
    p.add_inst(SMC_END());

    platform.execute(p);
  }

  uint Tester::exp_time_ns(uint hammer_count, uint RAS_scale, uint RP_scale, uint num_agg_rows, uint extra_cycles)
  {
    if (extra_cycles == 0)
      return (5 * (RAS_scale) + 5 * (RP_scale) + 9) * 6 * hammer_count * num_agg_rows;
    else
      return (9 + extra_cycles) * 6 * hammer_count * num_agg_rows;
  }

  uint Tester::asym_exp_time_ns(uint head_hammer_count, uint head_RAS_scale, uint tail_hammer_count, uint tail_RAS_scale, uint num_agg_rows)
  {
    return (9 + 5 * (head_RAS_scale)) * 6 * head_hammer_count * num_agg_rows + (9 + 5 * (tail_RAS_scale)) * 6 * tail_hammer_count * num_agg_rows;
  }

  uint Tester::max_hammer_count(uint prog_time_ns, uint RAS_scale, uint RP_scale, uint num_agg_rows)
  {
    uint max_hammer_count = std::ceil((double)prog_time_ns / (double)num_agg_rows / 6.0 / (5.0 * (RAS_scale) + 5.0 * (RP_scale) + 9.0));
    return max_hammer_count;
  }

  uint Tester::max_RAS_scale(uint prog_time_ns, uint hammer_count, uint RP_scale, uint num_agg_rows)
  {
    double round_time = std::ceil((double)prog_time_ns / (double)hammer_count / (double)num_agg_rows / 6.0);
    uint max_RAS_scale = std::ceil((round_time - 9.0 - 5.0 * (RP_scale)) / 5.0);

    return max_RAS_scale;
  }

  void Tester::SMC_add_sleep(Program& p, uint64_t sleep_cycles)
  {
    uint mult = sleep_cycles / std::numeric_limits<uint32_t>::max();
    uint32_t remainder = sleep_cycles - mult * std::numeric_limits<uint32_t>::max();

    for (uint i = 0; i < mult; i++)
      p.add_inst(SMC_SLEEP(std::numeric_limits<uint32_t>::max()));

    p.add_inst(SMC_SLEEP(remainder));
  };

  void Tester::SMC_retention_row(const CacheLine_t& cacheline, uint bank, uint row, uint64_t sleep_cycles)
  {
    Program p;
    // Initialize the row
    // Load bank and row address registers
    p.add_inst(SMC_LI(bank, BAR));
    p.add_inst(SMC_LI(row, RAR));
    // Column address stride is 8 since we are doing BL=8
    p.add_inst(SMC_LI(8, CASR));

    // Load the cache line into the wide data register
    for(uint i = 0 ; i < 16 ; i++)
    {
      p.add_inst(SMC_LI(cacheline[i], PATTERN_REG));
      p.add_inst(SMC_LDWD(PATTERN_REG, i));
    }

    // Activate and write to the row
    p.add_inst(SMC_PRE(BAR, 0, 0), SMC_NOP(), SMC_NOP(), SMC_NOP());
    p.add_inst(SMC_LI(0, CAR));
    p.add_inst(all_nops()); 
    p.add_inst(all_nops()); 

    p.add_inst(SMC_ACT(BAR, 0, RAR, 0), SMC_NOP(), SMC_NOP(), SMC_NOP());
    p.add_inst(all_nops()); 
    p.add_inst(all_nops());

    for(int i = 0 ; i < 128 ; i++)
    {
      p.add_inst(SMC_WRITE(BAR, 0, CAR, 1, 0, 0), SMC_NOP(), SMC_NOP(), SMC_NOP());
      p.add_inst(all_nops()); 
    }
    p.add_inst(SMC_SLEEP(4));
    
    p.add_inst(SMC_PRE(BAR, 0, 0), SMC_NOP(), SMC_NOP(), SMC_NOP());
    p.add_inst(all_nops()); 
    p.add_inst(all_nops());

    // Sleep for sleep_cycles * 6 nanoseconds
    SMC_add_sleep(p, sleep_cycles);

    // Read back the row
    p.add_inst(SMC_PRE(BAR, 0, 0), SMC_NOP(), SMC_NOP(), SMC_NOP());
    p.add_inst(SMC_LI(0, CAR));
    p.add_inst(all_nops());
    p.add_inst(all_nops());

    p.add_inst(SMC_ACT(BAR, 0, RAR, 0), SMC_NOP(), SMC_NOP(), SMC_NOP());
    p.add_inst(all_nops()); 
    p.add_inst(all_nops());

    for(int i = 0 ; i < 128 ; i++)
    {
      p.add_inst(SMC_READ(BAR, 0, CAR, 1, 0, 0), SMC_NOP(), SMC_NOP(), SMC_NOP());
      p.add_inst(all_nops()); 
    }
    p.add_inst(SMC_SLEEP(4));

    p.add_inst(SMC_PRE(BAR, 0, 0), SMC_NOP(), SMC_NOP(), SMC_NOP());
    p.add_inst(SMC_SLEEP(3));

    p.add_inst(SMC_END());
    
    platform.execute(p);    
  };


  void Tester::SMC_retention_rows(const std::vector<Row_t>& victim_rows, uint bank, uint64_t sleep_cycles)
  {
    Program p;

    // Store the victim row addresses
    uint num_rows = victim_rows.size();
    uint num_rows_reg = 7;
    uint row_ptr_reg = 8;
    uint row_reg = 9;

    p.add_inst(SMC_LI(num_rows, num_rows_reg));
    p.add_inst(SMC_LI(0, row_ptr_reg));
    p.add_inst(SMC_LI(8, CASR));

    for (uint i = 0; i < num_rows; i++)
    {
      p.add_inst(SMC_LI((uint32_t) victim_rows[i].row_address, row_reg));
      p.add_inst(SMC_ST(row_ptr_reg, i, row_reg));
    }

    // Activate the victim rows once to make them strong
    p.add_inst(SMC_LI(bank, BAR));
    for (uint i = 0; i < victim_rows.size(); i++) 
    {
      p.add_inst(SMC_LI(victim_rows[0].row_address, RAR));
      p.add_inst(SMC_ACT(BAR, 0, RAR, 0), SMC_NOP(), SMC_NOP(), SMC_NOP());
      p.add_inst(SMC_SLEEP(6));
      p.add_inst(SMC_PRE(BAR, 0, 0), SMC_NOP(), SMC_NOP(), SMC_NOP());
      p.add_inst(all_nops());
    }

    // Sleep for sleep_cycles * 6 nanoseconds
    SMC_add_sleep(p, sleep_cycles);

    // Read back the rows
    p.add_label("RD_ROW_BEGIN");
    p.add_inst(SMC_LD(row_ptr_reg, 0, RAR));
    p.add_inst(SMC_ADDI(row_ptr_reg, 1, row_ptr_reg));

    p.add_inst(SMC_ACT(BAR, 0, RAR, 0), SMC_NOP(), SMC_NOP(), SMC_NOP());
    p.add_inst(SMC_LI(0, CAR));
    p.add_inst(all_nops());

    for(int i = 0 ; i < 128 ; i++)
    {
      p.add_inst(SMC_READ(BAR, 0, CAR, 1, 0, 0), SMC_NOP(), SMC_NOP(), SMC_NOP());
      p.add_inst(all_nops()); 
    }
    p.add_inst(SMC_SLEEP(4));

    p.add_inst(SMC_PRE(BAR, 0, 0), SMC_NOP(), SMC_NOP(), SMC_NOP());
    p.add_branch(p.BR_TYPE::BL, row_ptr_reg, num_rows_reg, "RD_ROW_BEGIN");

    p.add_inst(SMC_END());

    platform.execute(p);
  };

};
#include "Refresh.h"

namespace Ramulator
{
  template <class T>
  class Controller;
  
  template<>
  RefreshBase<DDR5>::RefreshBase(const YAML::Node& config, Controller<DDR5>* ctrl):
  ctrl(ctrl)
  {

  }

  template <>
  class GenericRefresh<DDR5> : public RefreshBase<DDR5>
  {
    public:
      uint num_ranks = 0;
      uint num_banks = 0;

      std::vector<std::vector<uint>> REF_counters;
      uint next_bank_to_refresh = 0;

      std::vector<std::vector<uint>> ACT_counters;
      uint RAAIMT = 10000;

      bool is_SBR = false;
      bool RFM_en = false;


    public:
      GenericRefresh(const YAML::Node& config, Controller<DDR5>* ctrl);
      virtual ~GenericRefresh() = default;
      virtual std::string to_string()      
      {
        return fmt::format("Generic (DDR5)\n"); 
      }

    public:
      virtual void tick() override;
      virtual void update_refresh_scheduler(DDR5::Command cmd, const vector<int>& addr_vec) override;

    protected:
      void refresh_rank(int rank, bool is_RFM = false);
      void refresh_bank(int rank, int bank, bool is_RFM = false);
  };


  GenericRefresh<DDR5>::GenericRefresh(const YAML::Node& config, Controller<DDR5>* ctrl):
  RefreshBase(config, ctrl)
  {
    is_SBR = config["SameBankRefresh"].as<bool>(false);
    RFM_en = config["RFM"].as<bool>(false);
    RAAIMT = config["RAAIMT"].as<uint>(10000);


    num_ranks = ctrl->channel->spec->org_entry.count[(int)DDR5::Level::Rank];
    num_banks = ctrl->channel->spec->org_entry.count[(int)DDR5::Level::Bank];

    REF_counters.resize(num_ranks);
    for (auto& bank_counters : REF_counters)
      bank_counters.resize(num_banks, 0);

    ACT_counters.resize(num_ranks);
    for (auto& bank_counters : ACT_counters)
      bank_counters.resize(num_banks, 0);


    refresh_interval = ctrl->channel->spec->speed_entry[uint(DDR5::TimingCons::nREFI)];
    if (is_SBR)
      refresh_interval /= num_banks;
  }


  void GenericRefresh<DDR5>::tick()
  {
    this->clk++;

    if (RFM_en)
    {
      for (uint r = 0; r < num_ranks; r++)
      {
        for (uint b = 0; b < num_banks; b++)
        {
          if (ACT_counters[r][b] > RAAIMT)
          {
            if (is_SBR)
            {
              refresh_bank(r, b, true);
              for (uint _b = 0; _b < num_banks; _b++)
                ACT_counters[r][_b] = 0;
            }
            else
            {
              refresh_rank(r, true);
              goto reset_RAA_AB;
            }
          }
        }

      reset_RAA_AB:
        for (uint b = 0; b < num_banks; b++)
          ACT_counters[r][b] = 0;
      }
    }


    if ((this->clk - refreshed) >= refresh_interval) 
    {
      this->ref_count ++;
      if (is_SBR)
      {
        for (uint r = 0; r < num_ranks; r++)
        {
          REF_counters[r][next_bank_to_refresh]++;
          refresh_bank(r, next_bank_to_refresh);
        }

        next_bank_to_refresh = (next_bank_to_refresh + 1) % num_banks;
      }
      else
      {
        for (uint r = 0; r < num_ranks; r++)
          refresh_rank(r, true);
      }

      refreshed = this->clk;
    }
  }

  void GenericRefresh<DDR5>::update_refresh_scheduler(DDR5::Command cmd, const vector<int>& addr_vec)
  {
    if (cmd == DDR5::Command::ACT)
    {
      uint rank = addr_vec[1];
      uint bank = addr_vec[3];
      ACT_counters[rank][bank]++;
    } 
  }

  void GenericRefresh<DDR5>::refresh_rank(int rank, bool is_RFM)
  {
    std::vector<int> addr_vec(int(DDR5::Level::MAX), -1);
    addr_vec[0] = this->ctrl->channel->id;
    addr_vec[1] = rank;

    Request req(addr_vec, Request::Type::REFRESH, nullptr);
    if (is_RFM)
      req.type = Request::Type::RFM;
    bool res = this->ctrl->enqueue(req);

    if (!res)
      assert(false && "Failed to enqueue a refresh request!");
  }

  void GenericRefresh<DDR5>::refresh_bank(int rank, int bank, bool is_RFM)
  {
    std::vector<int> addr_vec(int(DDR5::Level::MAX), -1);
    addr_vec[0] = this->ctrl->channel->id;
    addr_vec[1] = rank;
    addr_vec[3] = bank;

    Request req(addr_vec, Request::Type::REFRESH_B, nullptr);
    if (is_RFM)
      req.type = Request::Type::RFM_B;

    bool res = this->ctrl->enqueue(req);

    if (!res)
      assert(false && "Failed to enqueue a refresh request!");
  }
}
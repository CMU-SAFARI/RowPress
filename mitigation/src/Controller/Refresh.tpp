#include "Refresh.h"

namespace Ramulator 
{
  template <class T>
  RefreshBase<T>* make_refresh(const YAML::Node& config, Controller<T>* ctrl)
  {
    if (!config["refresh"])
      throw std::runtime_error("Refresh scheduler configuration (refresh:) not found!");

    const YAML::Node& refresh_config = config["refresh"];
    std::string refresh_type = refresh_config["type"].as<std::string>("Generic");

    RefreshBase<T>* refresh = nullptr;

    if      (refresh_type == "Generic")
      refresh = new GenericRefresh<T>(refresh_config, ctrl);
    else
      throw std::runtime_error(fmt::format("Unrecognized refresh scheduler type: {}", refresh_type));

    std::cout << fmt::format("Refresh scheduler: {}", refresh->to_string()) << std::endl;
    return refresh;
  }


  template <class T>
  GenericRefresh<T>::GenericRefresh(const YAML::Node& config, Controller<T>* ctrl): 
  RefreshBase<T>(config, ctrl)
  {
    this->refresh_interval = ctrl->channel->spec->speed_entry[uint(T::TimingCons::nREFI)];
  }


  template <class T>
  void GenericRefresh<T>::tick()
  {
    this->clk++;

    if ((this->clk - this->refreshed) >= this->refresh_interval) 
    {
      this->ref_count ++;
      for (auto rank :this-> ctrl->channel->children)
        refresh_rank(rank->id);

      this->refreshed = this->clk;
    }
  }


  template <class T>
  void GenericRefresh<T>::refresh_rank(int rank)
  {
    std::vector<int> addr_vec(int(T::Level::MAX), -1);
    addr_vec[0] = this->ctrl->channel->id;
    addr_vec[1] = rank;

    Request req(addr_vec, Request::Type::REFRESH, nullptr);
    bool res = this->ctrl->enqueue(req);

    if (!res)
      assert(false && "Failed to enqueue a refresh request!");
  }
}

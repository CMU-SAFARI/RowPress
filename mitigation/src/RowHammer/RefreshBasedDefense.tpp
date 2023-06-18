#include "RefreshBasedDefense.h"
#include "PARA.h"
#include "Graphene.h"
#include "SSRD.h"

namespace Ramulator{  
  template <class T>
  RefreshBasedDefense<T>* make_refresh_based_defense(const YAML::Node& config, Controller<T>* ctrl)
  {
    if (!config["refresh_based_defense"])
        return nullptr;

    const YAML::Node& rbd_config = config["refresh_based_defense"];
    std::string rbd_type = rbd_config["type"].as<std::string>("PARA");

    RefreshBasedDefense<T>* rbd = nullptr;

    if (rbd_type == "PARA")
      rbd = new PARA<T>(rbd_config, ctrl);
    else if (rbd_type == "Graphene")
      rbd = new Graphene<T>(rbd_config, ctrl);
    else if (rbd_type == "SSRD")
      rbd = new SSRD<T>(rbd_config, ctrl);
    else
      throw std::runtime_error(fmt::format("Unrecognized refresh based defense type: {}", rbd_type));

    std::cout << fmt::format("Refresh based defense: {}", rbd->to_string()) << std::endl;
    return rbd;
  }
};
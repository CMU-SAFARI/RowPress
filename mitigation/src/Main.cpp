#include "Simulation.h"

using namespace Ramulator;

int main(int argc, char *argv[])
{
  const YAML::Node& config = Config::parse_config(argc, argv);

  if (config["processor"])
    run_cputraces(config);
  else
    run_memorytraces(config);

  return 0;
}

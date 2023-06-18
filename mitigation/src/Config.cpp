#include "Config.h"

using namespace Ramulator;
namespace fs = std::filesystem;

YAML::Node Config::load_file(std::string path_str)
{
  fs::path path(path_str);

  if (fs::exists(path))
  {
    YAML::Node node = YAML::LoadFile(path);
    fs::current_path(path.parent_path());
    return node;
  }
  else
  {
    std::cout << "File " << path_str << " does not exist!" << std::endl;
    exit(-1);
  }
}

void Config::traverse_node(YAML::Node node, bool verbose)
{
  switch (node.Type()) 
  {
    case YAML::NodeType::Scalar: 
      if (node.Tag() == "!include")
      {
        fs::path curr_path = fs::current_path();
        node = load_file(node.as<std::string>());
        traverse_node(node, verbose);
        fs::current_path(curr_path);
      }
      if (verbose)
        std::cout << node.as<std::string>() << std::endl;
      break;

    case YAML::NodeType::Sequence: 
      for (YAML::const_iterator seq_it = node.begin(); seq_it != node.end(); ++seq_it)
        traverse_node(*seq_it, verbose);
      break;

    case YAML::NodeType::Map: 
      for (YAML::const_iterator map_it = node.begin(); map_it != node.end(); ++map_it)
      {
        if (verbose)
          std::cout << map_it->first << ": " << std::endl;
        traverse_node(map_it->second, verbose);
      }
      break;

    case YAML::NodeType::Null:
      [[fallthrough]];
    case YAML::NodeType::Undefined: 
      break;
  }
}

void Config::override_configs(YAML::Node config, std::map<std::string, std::string>& params)
{
  for (const auto& [key, value] : params) 
  {
    std::stringstream ss(key);
    std::string token;
    std::vector<std::string> tokens;

    while (std::getline(ss, token, '.')) 
      tokens.push_back(std::move(token));

    YAML::Node node;
    node.reset(config);
    for (const auto& token : tokens)
    {
      // Search for array index
      std::vector<uint> indices;

      std::regex match_brackets("\\[(\\d+)]");
      std::sregex_iterator it(token.begin(), token.end(), match_brackets);
      std::sregex_iterator end;

      while(it != end)
      {
        indices.push_back(std::stoi((*it)[1]));
        it++;
      }

      // We don't have array indices in this token
      if (indices.empty())
      {
        if (!node[token]) 
          node[token] = YAML::Node(YAML::NodeType::Map);   
        node.reset(node[token]);
      }
      else
      {
        if (indices.size() > 1)
        {
          std::cerr << "Error! Nested sequence access is currently not supported!" << std::endl;
          exit(-1);
        }
        // Get the key of the map by removing the indices
        std::string _key = std::regex_replace(token, match_brackets, "");

        if (!node[_key]) 
          node[_key] = YAML::Node(YAML::NodeType::Sequence);
        else if (node[_key].Type() != YAML::NodeType::Sequence)
        {
          std::cerr << "Error! Node " << _key << " is not a sequence!" << std::endl;
          exit(-1);
        }
        node.reset(node[_key]);

        for (auto& i : indices)
        {
          if (i > node.size())
          {
            std::cerr << "Sequence access out of bound! To append elements to a sequence, use the index as one past the end of the sequence!" << std::endl;
            exit(-1);
          }
          node.reset(node[i]);
        }
      }
    }
    node = value;
    node.reset(config);
  }
}

static struct option long_options[] = 
{
  {"config",  required_argument, 0,  'c'},
  {"param",   required_argument, 0,  'p'},
  {0,         0,                 0,   0 }
};

YAML::Node Config::parse_config(int argc, char *argv[])
{
  const fs::path curr_path(fs::current_path());

  YAML::Node config;

  bool use_config_file = false;
  std::string config_filename;

  std::map<std::string, std::string> params;
  std::vector<std::string> traces;

  int option, longindex;
  while ((option = getopt_long_only(argc, argv, "", long_options, &longindex)) != -1) 
  {
    switch(option) {
      case 'c':
        use_config_file = true;
        config_filename = optarg;
        break;
      case 'p':
        {
          std::stringstream ss(optarg);
          std::string token;
          std::vector<std::string> tokens;

          while (std::getline(ss, token, '=')) 
            tokens.push_back(std::move(token));
          
          if (tokens.size() != 2)
          {
            std::cerr << "Invalid command parameter: " << optarg << std::endl;
            exit(-1);
          }

          params[tokens[0]] = tokens[1]; 

          break;
        }
      case ':':
      case '?':
        print_help(); 
        exit(-1);
        break;
      default:
        break; 
    }
  }

  // If the YAML config being supplied directly as an argument (being driven by external script)
  if (optind == (argc - 1)) 
  {
    if (use_config_file)
    {
      std::cerr << "Configuration already specified with option --config!" << std::endl;
      exit(-1);
    }

    // If so we do not accept any command line options
    config = YAML::Load(argv[optind]);
  }
  // Used standalone, need to recursively handle the "!include" tag.
  else
  {
    config = load_file(config_filename);
    traverse_node(config);
    override_configs(config, params);
  }

  fs::current_path(curr_path);

  return config;
};

void Config::print_help()
{
  std::cout << "HELP MSG PLACEHOLDER" << std::endl;
}

size_t Config::parse_capacity(std::string size_str)
{
  std::string suffixes[3] = {"KB", "MB", "GB"};
  for (int i = 0; i < 3; i++)
  {
    std::string suffix = suffixes[i];
    if (size_str.find(suffix) != std::string::npos)
    {
      size_t size = std::stoull(size_str);
      size = size << (10 * (i + 1));
      return size;
    }
  }
  return 0;
}

size_t Config::parse_frequency(std::string size_str)
{
  std::string suffixes[2] = {"MHz", "GHz"};
  for (int i = 0; i < 2; i++)
  {
    std::string suffix = suffixes[i];
    if (size_str.find(suffix) != std::string::npos)
    {
      size_t freq = std::stoull(size_str);
      freq = freq << (10 * i);
      return freq;
    }
  }
  return 0;
}

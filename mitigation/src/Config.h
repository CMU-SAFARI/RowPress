#pragma once

#include <getopt.h>
#include <regex>
#include <iostream>
#include <filesystem>

#include <yaml-cpp/yaml.h>

#include <spdlog/spdlog.h>


namespace Ramulator
{
  namespace Config
  {
    /// Top-level function to parse the YAML config file and the command line options
    YAML::Node parse_config(int argc, char *argv[]);
    void print_help();

    /**
     * @brief    Load and parse the YAML configuration file for the simulation.
     * 
     * @param    path           Path to the yaml file.
     * @return   YAML::Node     A YAML node containing all configurations.
     */
    YAML::Node load_file(std::string path);

    /**
     * @brief    Depth-first traversal of the YAML document to load any dependent YAML files.
     * 
     * @param    node           The current root node.
     * @param    verbose        Whether to print every node during the traversal.
     */
    void traverse_node(YAML::Node node, bool verbose=false);

    /**
     * @brief    Override the config (add if non-existent) in the YAML file with the command line options.
     * 
     * @param    config         Parsed YAML configurations.
     * @param    params         Command line option names and values.
     */
    void override_configs(YAML::Node config, std::map<std::string, std::string>& params);

    /**
     * @brief    Parse capacity strings (e.g., KB, MB) into the number of bytes
     * 
     * @param    size_str       A capacity string (e.g., "8KB", "64MB").
     * @return   size_t         The number of bytes.
     */
    size_t parse_capacity(std::string size_str);

    /**
     * @brief    Parse frequency strings (e.g., MHz, GHz) into MHz
     * 
     * @param    size_str       A capacity string (e.g., "4GHz", "3500MHz").
     * @return   size_t         The number of bytes.
     */
    size_t parse_frequency(std::string size_str);
  }
}
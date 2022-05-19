#include "../submodules/yaml-cpp/include/yaml-cpp/yaml.h"
#include "JsonKeyConfig.hpp"
#include <iostream>
using std::string;
using std::invalid_argument;

void JsonKeyConfig::parse_config_file (const string& config_file_path) {
    YAML::Node config = YAML::LoadFile(config_file_path);
 
    if (config.Type() != YAML::NodeType::Sequence) {
        throw invalid_argument("file format is wrong");
    }

    for (YAML::const_iterator it = config.begin(); it != config.end(); ++it) {
        auto node = (*it).begin();
        m_preparsed_keys[node->first.as<string>()] = node->second.as<string>();
    }
}
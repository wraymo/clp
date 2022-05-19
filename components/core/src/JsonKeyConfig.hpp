#ifndef JSONKEYCONFIG_HPP
#define JSONKEYCONFIG_HPP

// C++ standard libraries
#include <string>
#include <map>

/**
 * Class encapsulating the global metadata database's configuration details
 */
class JsonKeyConfig {
public:
    // Constructors
    JsonKeyConfig () {}

    // Methods
    void parse_config_file (const std::string& config_file_path);
    std::map<std::string, std::string> get_parsed_keys () const { return m_preparsed_keys; }

private:
    std::map<std::string, std::string> m_preparsed_keys;
};

#endif // JSONKEYCONFIG_HPP

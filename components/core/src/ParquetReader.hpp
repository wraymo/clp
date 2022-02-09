#ifndef PARQUETREADER_HPP
#define PARQUETREADER_HPP

#include <vector>

#include "JsonTypeDictionaryReader.hpp"
#include "LogTypeDictionaryReader.hpp"
#include "VariableDictionaryReader.hpp"

#include <boost/filesystem.hpp>

class ParquetReader {
public:
    ParquetReader () {}
    void open (const boost::filesystem::path& path);

private:
    LogTypeDictionaryReader m_logtype_dict;
    JsonTypeDictionaryReader m_jsontype_dict;
    VariableDictionaryReader m_var_dict;

    std::vector<InputFile*> m_files;
};

#endif //PARQUETREADER_HPP

#ifndef PARQUETWRITER_HPP
#define PARQUETWRITER_HPP

#include <string>
#include <unordered_map>
#include <vector>

#include <arrow/io/file.h>
#include <boost/uuid/uuid.hpp>
#include <boost/filesystem.hpp>
#include <parquet/properties.h>

#include "Defs.h"
#include "JsonTypeDictionaryEntry.hpp"
#include "LogTypeDictionaryEntry.hpp"
#include "VariableDictionaryEntry.hpp"

class ParquetWriter {
public:
    void init (boost::uuids::uuid id, boost::uuids::uuid creator_id, int compression_level, const boost::filesystem::path& output_dir);
    void add_metadata (const std::string& key, const std::string& value);

private:
    std::shared_ptr<parquet::schema::GroupNode> get_schema ();

    std::unordered_map<std::string, JsonTypeDictionaryEntry*> m_value_to_jsontype_entry;
    std::unordered_map<std::string, LogTypeDictionaryEntry*> m_value_to_logtype_entry;
    std::unordered_map<std::string, VariableDictionaryEntry*> m_value_to_variable_entry;

    std::unordered_map<jsontype_dictionary_id_t, JsonTypeDictionaryEntry*> m_id_to_jsontype_entry;
    std::unordered_map<logtype_dictionary_id_t, LogTypeDictionaryEntry*> m_id_to_logtype_entry;
    std::unordered_map<variable_dictionary_id_t, VariableDictionaryEntry*> m_id_to_variable_entry;

    std::vector<std::string> m_keys;
    std::vector<std::string> m_values;

    std::string m_creator_id_as_string;
    std::shared_ptr<arrow::io::FileOutputStream> m_outfile;
    parquet::WriterProperties::Builder m_builder;
};

#endif //PARQUETWRITER_HPP

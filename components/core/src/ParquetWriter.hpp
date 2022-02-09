#ifndef PARQUETWRITER_HPP
#define PARQUETWRITER_HPP

#include <vector>
#include <unordered_set>

#include <arrow/io/file.h>
#include <boost/uuid/uuid.hpp>
#include <boost/filesystem.hpp>
#include <parquet/properties.h>

#include <arrow/api.h>
#include <arrow/result.h>

#include "Defs.h"
#include "InputFile.hpp"
#include "JsonTypeDictionaryWriter.hpp"
#include "LogTypeDictionaryWriter.hpp"
#include "VariableDictionaryWriter.hpp"

using arrow::Int64Builder;
using arrow::ListBuilder;

constexpr int ROWGROUP_SIZE = 10000;

class ParquetWriter {
public:
    ParquetWriter (): m_rowgroup(0), m_row(0), m_compression_level(3), m_logtype_dict_offset(0), m_jsontype_dict_offset(0), m_var_dict_offset(0),
                        m_timestamp_builder(nullptr), m_id_builder(nullptr), m_variable_builder(nullptr), m_varialbe_item_builder(nullptr) {}
    ~ParquetWriter();

    void open (boost::uuids::uuid id, boost::uuids::uuid creator_id, int compression_level, const boost::filesystem::path& output_dir);
    void close ();

    InputFile* create_and_open_input_file(std::string path_for_compression);
    void close_input_file(InputFile* file) const;

    void write_msg(epochtime_t timestamp, const std::string& message);
    void write_json_msg(epochtime_t timestamp, ordered_json& message);

private:
    void add_metadata (const std::string& key, const std::string& value);
    void write_encoded_msg (epochtime_t timestamp, int64_t id, const std::vector<encoded_variable_t>& encoded_vars);
    void get_compressed_string (const std::string& src, std::string& dst);
    std::string get_file_metadata ();
    void get_logtype_dict_metadata (std::string& dict_value, std::string& dict_offset);
    void get_jsontype_dict_metadata (std::string& dict_value, std::string& dict_offset);
    void get_variable_dict_metadata (std::string& dict_value, std::string& dict_offset);
    void write_table ();

    std::string m_creator_id_as_string;
    std::string m_output_path;
    std::shared_ptr<arrow::io::FileOutputStream> m_outfile;
    int m_compression_level;

    std::vector<std::string> m_keys;
    std::vector<std::string> m_values;

    Int64Builder* m_timestamp_builder;
    Int64Builder* m_id_builder;
    ListBuilder* m_variable_builder;
    Int64Builder* m_varialbe_item_builder ;

    LogTypeDictionaryWriter m_logtype_dict;
    JsonTypeDictionaryWriter m_jsontype_dict;
    VariableDictionaryWriter m_var_dict;

    uint64_t m_logtype_dict_offset;
    uint64_t m_jsontype_dict_offset;
    uint64_t m_var_dict_offset;

    std::unique_ptr<LogTypeDictionaryEntry> m_logtype_dict_entry_wrapper;
    std::unique_ptr<JsonTypeDictionaryEntry> m_jsontype_dict_entry_wrapper;

    uint64_t m_rowgroup;
    uint64_t m_row;

    std::unordered_set<InputFile*> m_files;
};

#endif //PARQUETWRITER_HPP

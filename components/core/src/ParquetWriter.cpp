#include "ParquetWriter.hpp"
#include "EncodedVariableInterpreter.hpp"

#include <iostream>
#include <utility>
#include <boost/uuid/uuid_io.hpp>
#include <parquet/exception.h>
#include <parquet/file_writer.h>
#include <parquet/api/reader.h>
#include <parquet/arrow/writer.h>
#include <parquet/arrow/reader.h>
#include <parquet/stream_writer.h>

ParquetWriter::~ParquetWriter () {
    delete m_timestamp_builder;
    delete m_id_builder;
    delete m_variable_builder;
    delete m_varialbe_item_builder;

    for (auto file: m_files) {
        delete file;
    }
}

void ParquetWriter::open (boost::uuids::uuid id, boost::uuids::uuid creator_id, int compression_level, const boost::filesystem::path& output_dir) {
    std::string file_name = boost::uuids::to_string(id) + ".parquet";
    auto output_path = output_dir / file_name;
    m_output_path = output_path.string();
    PARQUET_ASSIGN_OR_THROW(m_outfile, arrow::io::FileOutputStream::Open(m_output_path));
    m_creator_id_as_string = boost::uuids::to_string(creator_id);
    m_compression_level = compression_level;

    arrow::MemoryPool* pool = arrow::default_memory_pool();
    m_timestamp_builder = new Int64Builder(pool);
    m_id_builder = new Int64Builder(pool);
    m_variable_builder = new ListBuilder(pool, std::make_shared<Int64Builder>(pool));
    m_varialbe_item_builder = (dynamic_cast<Int64Builder*>(m_variable_builder->value_builder()));

    m_jsontype_dict_entry_wrapper = std::make_unique<JsonTypeDictionaryEntry>();
    m_logtype_dict_entry_wrapper = std::make_unique<LogTypeDictionaryEntry>();

    m_jsontype_dict.open(cJsontypeDictionaryIdMax);
    m_logtype_dict.open(cLogtypeDictionaryIdMax);
    m_var_dict.open(cVariableDictionaryIdMax);
}

void ParquetWriter::close () {
    write_table();

    m_logtype_dict.close();
    m_jsontype_dict.close();
    m_var_dict.close();

    m_logtype_dict_entry_wrapper.reset(nullptr);
    m_logtype_dict_entry_wrapper.reset(nullptr);

    m_keys.clear();
    m_values.clear();

    m_timestamp_builder->Reset();
    m_id_builder->Reset();
    m_variable_builder->Reset();
    m_varialbe_item_builder->Reset();

    m_compression_level = 3;
    m_output_path.clear();
    m_creator_id_as_string.clear();
    m_outfile->Close();

    m_logtype_dict_offset = 0;
    m_jsontype_dict_offset = 0;
    m_var_dict_offset = 0;

    m_rowgroup = 0;
    m_row = 0;

    for (auto file: m_files) {
        delete file;
    }
    m_files.clear();
}

InputFile* ParquetWriter::create_and_open_input_file (std::string path) {
    auto file = new InputFile(std::move(path), m_rowgroup, m_row);
    m_files.insert(file);

    return file;
}

void ParquetWriter::close_input_file (InputFile* file) const {
    file->close(m_rowgroup, m_row);
}

void ParquetWriter::write_msg (epochtime_t timestamp, const std::string& message) {
    std::vector<encoded_variable_t> encoded_vars;
    EncodedVariableInterpreter::encode_and_add_to_dictionary(message, *m_logtype_dict_entry_wrapper, m_var_dict, encoded_vars);
    logtype_dictionary_id_t logtype_id;
    if (m_logtype_dict.add_occurrence(m_logtype_dict_entry_wrapper, logtype_id)) {
        m_logtype_dict_entry_wrapper = std::make_unique<LogTypeDictionaryEntry>();
    }

    write_encoded_msg(timestamp, logtype_id, encoded_vars);
}

void ParquetWriter::write_json_msg (epochtime_t timestamp, ordered_json& message) {
    std::vector<encoded_variable_t> encoded_vars;
    EncodedVariableInterpreter::encode_and_add_to_dictionary(message, *m_jsontype_dict_entry_wrapper, m_logtype_dict, m_var_dict, encoded_vars);
    jsontype_dictionary_id_t jsontype_id;
    if (m_jsontype_dict.add_occurrence(m_jsontype_dict_entry_wrapper, jsontype_id)) {
        m_jsontype_dict_entry_wrapper = std::make_unique<JsonTypeDictionaryEntry>();
    }

    write_encoded_msg(timestamp, jsontype_id, encoded_vars);
}

void ParquetWriter::add_metadata (const std::string& key, const std::string& value) {
    m_keys.push_back(key);
    m_values.push_back(value);
}

void ParquetWriter::write_encoded_msg (epochtime_t timestamp, int64_t id, const std::vector<encoded_variable_t>& encoded_vars) {
    if (m_row >= ROWGROUP_SIZE) {
        m_rowgroup++;
        m_row = 0;
    }

    m_timestamp_builder->Append(timestamp);
    m_id_builder->Append(id);
    m_variable_builder->Append();
    m_varialbe_item_builder->AppendValues(encoded_vars);
}

void ParquetWriter::get_compressed_string (const std::string& src, std::string& dst) {
    size_t const size = ZSTD_compressBound(src.size());
    dst.resize(size);

    auto dstp = const_cast<void*>(static_cast<const void*>(dst.c_str()));
    auto srcp = static_cast<const void*>(src.c_str());

    size_t const compressed_size = ZSTD_compress(dstp, size, srcp, src.size(), m_compression_level);
    auto code = ZSTD_isError(compressed_size);
    if (code) {
        return;
    }
    dst.resize(compressed_size);
}

std::string ParquetWriter::get_file_metadata () {
    std::string file_metadata_str;

    for (auto file: m_files) {
        std::string path = file->get_path();
        size_t length = path.length();
        file_metadata_str += std::string(static_cast<char*>(static_cast<void*>(&length)), sizeof(size_t));
        file_metadata_str += path;

        if (file->get_type() == InputFile::FileType::TEXT)
            file_metadata_str += "text";
        else
            file_metadata_str += "json";

        uint64_t start_rowgroup, start_row, end_rowgroup, end_row;
        start_rowgroup = file->get_start_rowgroup();
        start_row = file->get_start_row();
        end_rowgroup = file->get_end_rowgroup();
        end_row = file->get_end_row();

        file_metadata_str += std::string(static_cast<char*>(static_cast<void*>(&start_rowgroup)), sizeof(uint64_t));
        file_metadata_str += std::string(static_cast<char*>(static_cast<void*>(&start_row)), sizeof(uint64_t));
        file_metadata_str += std::string(static_cast<char*>(static_cast<void*>(&end_rowgroup)), sizeof(uint64_t));
        file_metadata_str += std::string(static_cast<char*>(static_cast<void*>(&end_row)), sizeof(uint64_t));
    }

    return file_metadata_str;
}

void ParquetWriter::write_table () {
    std::string logtype_dict_value_str, logtype_dict_offset_str, logtype_dict_compressed_value_str, logtype_dict_compressed_offset_str;
    std::string jsontype_dict_value_str, jsontype_dict_offset_str, jsontype_dict_compressed_value_str, jsontype_dict_compressed_offset_str;
    std::string variable_dict_value_str, variable_dict_offset_str, variable_dict_compressed_value_str, variable_dict_compressed_offset_str;

    get_logtype_dict_metadata(logtype_dict_value_str, logtype_dict_offset_str);
    get_compressed_string(logtype_dict_value_str, logtype_dict_compressed_value_str);
    get_compressed_string(logtype_dict_offset_str, logtype_dict_compressed_offset_str);
    add_metadata("logtype_value", logtype_dict_compressed_value_str);
    add_metadata("logtype_offset", logtype_dict_compressed_offset_str);

    get_jsontype_dict_metadata(jsontype_dict_value_str, jsontype_dict_offset_str);
    get_compressed_string(jsontype_dict_value_str, jsontype_dict_compressed_value_str);
    get_compressed_string(jsontype_dict_offset_str, jsontype_dict_compressed_offset_str);
    add_metadata("jsontype_value", jsontype_dict_compressed_value_str);
    add_metadata("jsontype_offset", jsontype_dict_compressed_offset_str);

    get_variable_dict_metadata(variable_dict_value_str, variable_dict_offset_str);
    get_compressed_string(variable_dict_value_str, variable_dict_compressed_value_str);
    get_compressed_string(variable_dict_offset_str, variable_dict_compressed_offset_str);
    add_metadata("variable_value", variable_dict_compressed_value_str);
    add_metadata("variable_offset", variable_dict_compressed_offset_str);

    add_metadata("file", get_file_metadata());
//    std::cout << "logtype size: " << logtype_dict_compressed_str.length() << std::endl;
//    std::cout << "jsontype size: " << jsontype_dict_compressed_str.length() << std::endl;
//    std::cout << "variable size: " << variable_dict_compressed_str.length() << std::endl;
//
//    std::cout << "logtype size: " << logtype_dict_str.length() << std::endl;
//    std::cout << "jsontype size: " << jsontype_dict_str.length() << std::endl;
//    std::cout << "variable size: " << variable_dict_str.length() << std::endl;

    std::shared_ptr<arrow::Array> timestamp_array;
    m_timestamp_builder->Finish(&timestamp_array);

    std::shared_ptr<arrow::Array> json_id_array;
    m_id_builder->Finish(&json_id_array);

    std::shared_ptr<arrow::Array> variable_array;
    m_variable_builder->Finish(&variable_array);

    std::vector<std::shared_ptr<arrow::Field>> schema_vector = { arrow::field("timestamp", arrow::int64()), arrow::field("json_id", arrow::int64()),
                                                                 arrow::field("variables", arrow::list(arrow::int64()))};
    auto table = arrow::Table::Make(std::make_shared<arrow::Schema>(schema_vector, arrow::KeyValueMetadata::Make(m_keys, m_values)),
            {timestamp_array, json_id_array, variable_array});

    auto properties = parquet::WriterProperties::Builder().compression(parquet::Compression::ZSTD)->compression_level(m_compression_level)->build();
    auto arrow_properties = parquet::ArrowWriterProperties::Builder().store_schema()->build();
    PARQUET_THROW_NOT_OK(parquet::arrow::WriteTable(*table, arrow::default_memory_pool(), m_outfile, 10000, properties, arrow_properties));

    std::cout << std::endl;
}

void ParquetWriter::get_logtype_dict_metadata (std::string& dict_value, std::string& dict_offset) {
    auto entries = m_logtype_dict.get_uncommitted_entries();

    for (auto& entry : entries) {
        int64_t id = entry->get_id();
        uint8_t verbosity = entry->get_verbosity();
        std::string value = entry->get_value();
        entry->get_value_with_unfounded_variables_escaped(value);
        size_t length = value.length() + sizeof(uint8_t);

        dict_value += std::string(static_cast<char*>(static_cast<void*>(&verbosity)), sizeof(uint8_t));
        dict_value += value;
        dict_offset += std::string(static_cast<char*>(static_cast<void*>(&m_logtype_dict_offset)), sizeof(uint64_t));
        m_logtype_dict_offset += length;
    }

    m_logtype_dict.reset_entries();
}

void ParquetWriter::get_jsontype_dict_metadata (std::string& dict_value, std::string& dict_offset) {
    auto entries = m_jsontype_dict.get_uncommitted_entries();

    for (auto& entry : entries) {
        int64_t id = entry->get_id();
        uint64_t num_vars = entry->get_num_vars();
        const std::string& value = entry->get_value();
        size_t length = value.length() + sizeof(uint64_t);

        dict_value += std::string(static_cast<char*>(static_cast<void*>(&num_vars)), sizeof(uint64_t));
        dict_value += value;
        dict_offset += std::string(static_cast<char*>(static_cast<void*>(&m_jsontype_dict_offset)), sizeof(uint64_t));
        m_jsontype_dict_offset += length;
    }

    m_logtype_dict.reset_entries();
}

void ParquetWriter::get_variable_dict_metadata (std::string& dict_value, std::string& dict_offset) {
    auto entries = m_var_dict.get_uncommitted_entries();

    for (auto& entry : entries) {
        uint64_t id = entry->get_id();
        const std::string& value = entry->get_value();
        size_t length = value.length();

        dict_value += value;
        dict_offset += std::string(static_cast<char*>(static_cast<void*>(&m_var_dict_offset)), sizeof(uint64_t));
        m_var_dict_offset += length;
    }

    m_var_dict.reset_entries();
}

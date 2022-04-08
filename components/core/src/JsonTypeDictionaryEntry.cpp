#include "JsonTypeDictionaryEntry.hpp"

// Project headers
#include "Utils.hpp"

using std::string;

void JsonTypeDictionaryEntry::add_logtype(ordered_json& value, logtype_dictionary_id_t& id) {
    string new_value;
    new_value += (char)Delim::LogType;
    new_value += std::to_string(id);

    value = new_value;
}

void JsonTypeDictionaryEntry::add_logtype(ordered_json& value) {
    value = string(1, (char)Delim::LogType);
}

void JsonTypeDictionaryEntry::add_string_var(ordered_json& value) {
    value = string(1, (char)Delim::StringVar);
}

void JsonTypeDictionaryEntry::add_non_double_var(ordered_json& value) {
    value = string(1, (char)Delim::NonDoubleVar);
}

void JsonTypeDictionaryEntry::add_double_var(ordered_json& value, uint8_t num_integer_digits, uint8_t num_fractional_digits) {
    if (num_integer_digits + num_fractional_digits > cMaxDigitsInRepresentableDoubleVar) {
        throw JsonTypeDictionaryEntry::OperationFailed(ErrorCode_BadParam, __FILENAME__, __LINE__);
    }

    num_integer_digits <<= 4ULL;
    num_integer_digits |= (num_fractional_digits & 0x0FU);

    string new_value;
    new_value += (char)Delim::DoubleVar;
    new_value += (char)num_integer_digits;
    value = new_value;
}

void JsonTypeDictionaryEntry::add_boolean_var(ordered_json &value) {
    value = string(1, (char)Delim::BooleanVar);
}

void JsonTypeDictionaryEntry::write_to_file (streaming_compression::zstd::Compressor& compressor) const {
    compressor.write_numeric_value(m_id);
    compressor.write_numeric_value<size_t>(m_num_vars);

    compressor.write_numeric_value<uint64_t>(m_value.length());
    compressor.write_string(m_value);
}

void JsonTypeDictionaryEntry::set(ordered_json &object) {
    m_value = object.dump();
}

size_t JsonTypeDictionaryEntry::get_data_size() const {
    return sizeof(m_id) + sizeof(m_num_vars) + m_value.length() + m_ids_of_segments_containing_entry.size() * sizeof(segment_id_t);
}

bool JsonTypeDictionaryEntry::is_string_var(string &str) {
    return !str.empty() && str[0] == (char)Delim::StringVar;
}

bool JsonTypeDictionaryEntry::is_non_double_var(string &str) {
    return !str.empty() && str[0] == (char)Delim::NonDoubleVar;
}

bool JsonTypeDictionaryEntry::is_double_var(string &str) {
    return !str.empty() && str[0] == (char)Delim::DoubleVar;
}

bool JsonTypeDictionaryEntry::is_boolean_var(string &str) {
    return !str.empty() && str[0] == (char)Delim::BooleanVar;
}

bool JsonTypeDictionaryEntry::is_logtype(string &str) {
    return !str.empty() && str[0] == (char)Delim::LogType;
}

logtype_dictionary_id_t JsonTypeDictionaryEntry::get_logtype_id(string &str) {
    return std::stol(str.substr(1));
}

ErrorCode JsonTypeDictionaryEntry::try_read_from_file (streaming_compression::zstd::Decompressor& decompressor) {
    m_value.clear();

    ErrorCode error_code;

    error_code = decompressor.try_read_numeric_value<jsontype_dictionary_id_t>(m_id);
    if (ErrorCode_Success != error_code) {
        return error_code;
    }

    error_code = decompressor.try_read_numeric_value<size_t>(m_num_vars);
    if (ErrorCode_Success != error_code) {
        return error_code;
    }

    uint64_t value_length;
    error_code = decompressor.try_read_numeric_value(value_length);
    if (ErrorCode_Success != error_code) {
        return error_code;
    }

    string value;
    error_code = decompressor.try_read_string(value_length, value);
    if (ErrorCode_Success != error_code) {
        return error_code;
    }

    m_value = std::move(value);
    return error_code;
}

void JsonTypeDictionaryEntry::read_from_file (streaming_compression::zstd::Decompressor& decompressor) {
    auto error_code = try_read_from_file(decompressor);
    if (ErrorCode_Success != error_code) {
        throw OperationFailed(error_code, __FILENAME__, __LINE__);
    }
}

void JsonTypeDictionaryEntry::set_num_vars(size_t num_vars) {
    m_num_vars = num_vars;
}

size_t JsonTypeDictionaryEntry::get_num_vars() const {
    return m_num_vars;
}

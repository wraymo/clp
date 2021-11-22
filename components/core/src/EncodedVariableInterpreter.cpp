#include "EncodedVariableInterpreter.hpp"

// C++ standard libraries
#include <cassert>
#include <cmath>

// spdlog
#include <spdlog/spdlog.h>

// Project headers
#include "Defs.h"
#include "Utils.hpp"

using std::string;
using std::unordered_set;
using std::vector;

// Constants
// 1 sign + LogTypeDictionaryEntry::cMaxDigitsInRepresentableDoubleVar + 1 decimal point
static const size_t cMaxCharsInRepresentableDoubleVar = LogTypeDictionaryEntry::cMaxDigitsInRepresentableDoubleVar + 2;

encoded_variable_t EncodedVariableInterpreter::get_var_dict_id_range_begin () {
    return m_var_dict_id_range_begin;
}

encoded_variable_t EncodedVariableInterpreter::get_var_dict_id_range_end () {
    return m_var_dict_id_range_end;
}

bool EncodedVariableInterpreter::is_var_dict_id (encoded_variable_t encoded_var) {
    return (m_var_dict_id_range_begin <= encoded_var && encoded_var < m_var_dict_id_range_end);
}

variable_dictionary_id_t EncodedVariableInterpreter::decode_var_dict_id (encoded_variable_t encoded_var) {
    variable_dictionary_id_t id = encoded_var - m_var_dict_id_range_begin;
    return id;
}

bool EncodedVariableInterpreter::convert_string_to_representable_integer_var (const string& value, encoded_variable_t& encoded_var) {
    size_t length = value.length();
    if (0 == length) {
        // Empty string cannot be converted
        return false;
    }

    // Ensure start of value is an integer with no zero-padding or positive sign
    if ('-' == value[0]) {
        // Ensure first character after sign is a non-zero integer
        if (length < 2 || value[1] < '1' || '9' < value[1]) {
            return false;
        }
    } else {
        // Ensure first character is a digit
        if (value[0] < '0' || '9' < value[0]) {
            return false;
        }

        // Ensure value is not zero-padded
        if (length > 1 && '0' == value[0]) {
            return false;
        }
    }

    int64_t result;
    if (false == convert_string_to_int64(value, result)) {
        // Conversion failed
        return false;
    } else if (result >= m_var_dict_id_range_begin) {
        // Value is in dictionary variable range, so cannot be converted
        return false;
    } else {
        encoded_var = result;
    }

    return true;
}

bool EncodedVariableInterpreter::convert_string_to_representable_double_var (const string& value, uint8_t& num_integer_digits, uint8_t& num_fractional_digits,
                                                                             encoded_variable_t& encoded_var)
{
    size_t length = value.length();

    // Check for preceding negative sign
    size_t first_digit_pos = 0;
    if (first_digit_pos < length && '-' == value[first_digit_pos]) {
        ++first_digit_pos;

        if (length > cMaxCharsInRepresentableDoubleVar) {
            // Too many characters besides sign to represent precisely
            return false;
        }
    } else {
        // No negative sign, so check against max size - 1
        if (length > cMaxCharsInRepresentableDoubleVar - 1) {
            // Too many characters to represent precisely
            return false;
        }
    }

    // Find decimal point
    size_t decimal_point_pos = string::npos;
    for (size_t i = first_digit_pos; i < length; ++i) {
        char c = value[i];
        if ('.' == c) {
            decimal_point_pos = i;
            break;
        } else if (!('0' <= c && c <= '9')) {
            // Unrepresentable double character
            return false;
        }
    }
    if (string::npos == decimal_point_pos) {
        // Decimal point doesn't exist
        return false;
    }

    num_integer_digits = decimal_point_pos - first_digit_pos;

    // Check that remainder of string is purely numbers
    for (size_t i = decimal_point_pos + 1; i < length; ++i) {
        char c = value[i];
        if (!('0' <= c && c <= '9')) {
            return false;
        }
    }

    num_fractional_digits = length - (decimal_point_pos + 1);

    double result;
    if (false == convert_string_to_double(value, result)) {
        // Conversion failed
        return false;
    } else {
        encoded_var = *reinterpret_cast<encoded_variable_t*>(&result);
    }

    return true;
}

void EncodedVariableInterpreter::encode_and_add_to_dictionary (const string& message, LogTypeDictionaryEntry& logtype_dict_entry,
                                                               VariableDictionaryWriter& var_dict, vector<encoded_variable_t>& encoded_vars)
{
    // Extract all variables and add to dictionary while building logtype
    size_t tok_begin_pos = 0;
    size_t next_delim_pos = 0;
    size_t last_var_end_pos = 0;
    string var_str;
    logtype_dict_entry.clear();
    // To avoid reallocating the logtype as we append to it, reserve enough space to hold the entire message
    logtype_dict_entry.reserve_constant_length(message.length());
    while (logtype_dict_entry.parse_next_var(message, tok_begin_pos, next_delim_pos, last_var_end_pos, var_str)) {
        // Encode variable
        encoded_variable_t encoded_var;
        uint8_t num_integer_digits;
        uint8_t num_fractional_digits;
        if (convert_string_to_representable_integer_var(var_str, encoded_var)) {
            logtype_dict_entry.add_non_double_var();
        } else if (convert_string_to_representable_double_var(var_str, num_integer_digits, num_fractional_digits, encoded_var)) {
            logtype_dict_entry.add_double_var(num_integer_digits, num_fractional_digits);
        } else {
            // Variable string looks like a dictionary variable, so encode it as so
            variable_dictionary_id_t id;
            var_dict.add_occurrence(var_str, id);
            encoded_var = encode_var_dict_id(id);

            logtype_dict_entry.add_non_double_var();
        }

        encoded_vars.push_back(encoded_var);
    }
}

void EncodedVariableInterpreter::encode_json_object_and_add_to_dictionary (ordered_json& object, JsonTypeDictionaryEntry& jsontype_dict_entry,
                                                                           LogTypeDictionaryWriter& logtype_dict, VariableDictionaryWriter& var_dict,
                                                                           vector<encoded_variable_t>& encoded_vars)
{
    for (auto i = object.begin(); i != object.end(); ++i) {
        if (i.value().is_object() || i.value().is_array()) {
            encode_json_object_and_add_to_dictionary(i.value(), jsontype_dict_entry, logtype_dict, var_dict, encoded_vars);
        } else {
            encoded_variable_t encoded_var;
            if (i.value().is_string()) {
                string str = i.value().get<string>();
                if (str.find(' ') == string::npos) {
                    variable_dictionary_id_t id;
                    var_dict.add_occurrence(str, id);
                    encoded_var = encode_var_dict_id(id);
                    encoded_vars.push_back(encoded_var);

                    JsonTypeDictionaryEntry::add_string_var(i.value());
                } else {
                    logtype_dictionary_id_t logtype_id;
                    std::unique_ptr<LogTypeDictionaryEntry> logtype_dict_entry = std::make_unique<LogTypeDictionaryEntry>();

                    encode_and_add_to_dictionary(str, *logtype_dict_entry, var_dict, encoded_vars);
                    logtype_dict.add_occurrence(logtype_dict_entry, logtype_id);
                    JsonTypeDictionaryEntry::add_logtype(i.value(), logtype_id);
                }
            } else if (i.value().is_number_float()) {
                uint8_t num_integer_digits;
                uint8_t num_fractional_digits;

                double value = i.value().get<double>();
                convert_string_to_representable_double_var(std::to_string(value), num_integer_digits, num_fractional_digits, encoded_var);

                JsonTypeDictionaryEntry::add_double_var(i.value(), num_integer_digits, num_fractional_digits);
                encoded_vars.push_back(encoded_var);
            } else if (i.value().is_number_integer()) {
                encoded_var = i.value().get<encoded_variable_t>();
                JsonTypeDictionaryEntry::add_non_double_var(i.value());

                encoded_vars.push_back(encoded_var);
            } else if (i.value().is_boolean()) {
                bool value = i.value().get<bool>();
                if (value) {
                    encoded_vars.push_back(1);
                } else {
                    encoded_vars.push_back(0);
                }

                JsonTypeDictionaryEntry::add_boolean_var(i.value());
            }
        }
    }
}

void EncodedVariableInterpreter::encode_and_add_to_dictionary (ordered_json& message, JsonTypeDictionaryEntry& jsontype_dict_entry,
                                                               LogTypeDictionaryWriter& logtype_dict, VariableDictionaryWriter& var_dict,
                                                               vector<encoded_variable_t>& encoded_vars)
{
    encode_json_object_and_add_to_dictionary(message, jsontype_dict_entry, logtype_dict, var_dict, encoded_vars);
    jsontype_dict_entry.set_num_vars(encoded_vars.size());
    jsontype_dict_entry.set(message);
}

bool EncodedVariableInterpreter::decode_variables_into_json_message (ordered_json& object, const LogTypeDictionaryReader &logtype_dict,
                                                                     const VariableDictionaryReader &var_dict, size_t &var_ix,
                                                                     const std::vector<encoded_variable_t> &encoded_vars)
{
    for (auto i = object.begin(); i != object.end(); ++i) {
        if (i.value().is_object() || i.value().is_array()) {
            if (!decode_variables_into_json_message(i.value(), logtype_dict, var_dict, var_ix, encoded_vars))
                return false;
        } else if (i.value().is_string()) {
            string str = i.value().get<string>();
            if (JsonTypeDictionaryEntry::is_string_var(str)) {
                auto var_dict_id = decode_var_dict_id(encoded_vars[var_ix]);
                i.value() = var_dict.get_value(var_dict_id);
                var_ix++;
            } else if (JsonTypeDictionaryEntry::is_logtype(str)) {
                logtype_dictionary_id_t logtype_id = JsonTypeDictionaryEntry::get_logtype_id(str);
                const LogTypeDictionaryEntry& logtype_entry = logtype_dict.get_entry(logtype_id);
                std::string decompressed_msg;
                if (!decode_variables_into_message (logtype_entry, var_dict, encoded_vars, var_ix, decompressed_msg, false))
                    return false;

                i.value() = decompressed_msg;
                var_ix += logtype_entry.get_num_vars();
            } else if (JsonTypeDictionaryEntry::is_double_var(str)) {
                double var_as_double = *reinterpret_cast<const double*>(&encoded_vars[var_ix]);
                char double_str[cMaxCharsInRepresentableDoubleVar + 1];

                auto encoded_value = (uint8_t)str[1];
                uint8_t num_integer_digits = encoded_value >> 4U;
                uint8_t num_fractional_digits = encoded_value & 0x0FU;
                int double_str_length = num_integer_digits + 1 + num_fractional_digits;

                if (std::signbit(var_as_double)) {
                    ++double_str_length;
                }
                snprintf(double_str, sizeof(double_str), "%0*.*f", double_str_length, num_fractional_digits, var_as_double);

                i.value() = std::stod(double_str);
                var_ix++;
            } else if (JsonTypeDictionaryEntry::is_non_double_var(str)) {
                if (!is_var_dict_id(encoded_vars[var_ix])) {
                    i.value() = encoded_vars[var_ix];
                    var_ix++;
                }
            } else if (JsonTypeDictionaryEntry::is_boolean_var(str)){
                if (encoded_vars[var_ix] == 1) {
                    i.value() = true;
                } else {
                    i.value() = false;
                }
                var_ix++;
            } else {
                SPDLOG_ERROR("EncodedVariableInterpreter: Invalid value type for key '{}'.", i.key());
                return false;
            }
        } else if (!i.value().is_null()){
            SPDLOG_ERROR("EncodedVariableInterpreter: Invalid value type for key '{}'.", i.key());
            return false;
        }
    }

    return true;
}

bool EncodedVariableInterpreter::decode_variables_into_message (const JsonTypeDictionaryEntry& jsontype_dict_entry, const LogTypeDictionaryReader &logtype_dict,
                                                                const VariableDictionaryReader &var_dict, const std::vector<encoded_variable_t> &encoded_vars,
                                                                std::string &decompressed_msg)
{
    const auto& jsontype_value = jsontype_dict_entry.get_value();

    ordered_json object = ordered_json::parse(jsontype_value);
    if (object.is_discarded()) {
        return false;
    }

    size_t var_ix = 0;
    bool res = decode_variables_into_json_message(object, logtype_dict, var_dict, var_ix, encoded_vars);
    if (!res) {
        return res;
    } else {
        decompressed_msg = object.dump();
        if (decompressed_msg.back() != '\n')
            decompressed_msg += '\n';
        return res;
    }
}

bool EncodedVariableInterpreter::decode_variables_into_message (const LogTypeDictionaryEntry& logtype_dict_entry, const VariableDictionaryReader& var_dict,
                                                                const vector<encoded_variable_t>& encoded_vars, size_t var_ix, string& decompressed_msg,
                                                                bool check)
{
    size_t num_vars_in_logtype = logtype_dict_entry.get_num_vars();

    // Ensure the number of variables in the logtype matches the number of encoded variables given
    const auto& logtype_value = logtype_dict_entry.get_value();
    if (check && num_vars_in_logtype != encoded_vars.size()) {
        SPDLOG_ERROR("EncodedVariableInterpreter: Logtype '{}' contains {} variables, but {} were given for decoding.", logtype_value.c_str(),
                     num_vars_in_logtype, encoded_vars.size());
        return false;
    }

    LogTypeDictionaryEntry::VarDelim var_delim;
    uint8_t num_integer_digits;
    uint8_t num_fractional_digits;
    size_t constant_begin_pos = 0;
    char double_str[cMaxCharsInRepresentableDoubleVar + 1];
    for (size_t i = 0; i < num_vars_in_logtype; ++i) {
        size_t var_position = logtype_dict_entry.get_var_info(i, var_delim, num_integer_digits, num_fractional_digits);

        // Add the constant that's between the last variable and this one
        decompressed_msg.append(logtype_value, constant_begin_pos, var_position - constant_begin_pos);

        if (LogTypeDictionaryEntry::VarDelim::NonDouble == var_delim) {
            if (!is_var_dict_id(encoded_vars[var_ix + i])) {
                decompressed_msg += std::to_string(encoded_vars[var_ix + i]);
            } else {
                auto var_dict_id = decode_var_dict_id(encoded_vars[var_ix + i]);
                decompressed_msg += var_dict.get_value(var_dict_id);
            }

            // Move past the variable delimiter
            constant_begin_pos = var_position + 1;
        } else { // LogTypeDictionaryEntry::VarDelim::Double == var_delim
            double var_as_double = *reinterpret_cast<const double*>(&encoded_vars[var_ix + i]);
            int double_str_length = num_integer_digits + 1 + num_fractional_digits;
            if (std::signbit(var_as_double)) {
                ++double_str_length;
            }
            snprintf(double_str, sizeof(double_str), "%0*.*f", double_str_length, num_fractional_digits, var_as_double);

            decompressed_msg += double_str;

            // Move past the variable delimiter and the double's precision
            constant_begin_pos = var_position + 2;
        }
    }
    // Append remainder of logtype, if any
    if (constant_begin_pos < logtype_value.length()) {
        decompressed_msg.append(logtype_value, constant_begin_pos, string::npos);
    }

    return true;
}

bool EncodedVariableInterpreter::encode_and_search_dictionary (const string& var_str, const VariableDictionaryReader& var_dict, bool ignore_case,
                                                               string& logtype, SubQuery& sub_query)
{
    size_t length = var_str.length();
    if (0 == length) {
        throw OperationFailed(ErrorCode_BadParam, __FILENAME__, __LINE__);
    }

    uint8_t num_integer_digits;
    uint8_t num_fractional_digits;
    encoded_variable_t encoded_var;
    if (convert_string_to_representable_integer_var(var_str, encoded_var)) {
        LogTypeDictionaryEntry::add_non_double_var(logtype);
        sub_query.add_non_dict_var(encoded_var);
    } else if (convert_string_to_representable_double_var(var_str, num_integer_digits, num_fractional_digits, encoded_var)) {
        LogTypeDictionaryEntry::add_double_var(num_integer_digits, num_fractional_digits, logtype);
        sub_query.add_non_dict_var(encoded_var);
    } else {
        auto entry = var_dict.get_entry_matching_value(var_str, ignore_case);
        if (nullptr == entry) {
            // Not in dictionary
            return false;
        }
        encoded_var = encode_var_dict_id(entry->get_id());

        LogTypeDictionaryEntry::add_non_double_var(logtype);
        sub_query.add_dict_var(encoded_var, entry);
    }

    return true;
}

bool EncodedVariableInterpreter::wildcard_search_dictionary_and_get_encoded_matches (const std::string& var_wildcard_str,
                                                                                     const VariableDictionaryReader& var_dict,
                                                                                     bool ignore_case, SubQuery& sub_query)
{
    // Find matches
    unordered_set<const VariableDictionaryEntry*> var_dict_entries;
    var_dict.get_entries_matching_wildcard_string(var_wildcard_str, ignore_case, var_dict_entries);
    if (var_dict_entries.empty()) {
        // Not in dictionary
        return false;
    }

    // Encode matches
    unordered_set<encoded_variable_t> encoded_vars;
    for (auto entry : var_dict_entries) {
        encoded_vars.insert(encode_var_dict_id(entry->get_id()));
    }

    sub_query.add_imprecise_dict_var(encoded_vars, var_dict_entries);

    return true;
}

encoded_variable_t EncodedVariableInterpreter::encode_var_dict_id (variable_dictionary_id_t id) {
    return (encoded_variable_t)id + m_var_dict_id_range_begin;
}

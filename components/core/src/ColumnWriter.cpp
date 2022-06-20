#include "ColumnWriter.hpp"
#include <iostream>

void Int64ColumnWriter::add_value(ordered_json* value) {
    // int64_t value1 = value->get<encoded_variable_t>();
    // std::cout << value1 << std::endl;
    // m_values.push_back(value.i);
    m_values.push_back(value->get<encoded_variable_t>());
}

char* Int64ColumnWriter::get_data() {
    return reinterpret_cast<char*>(m_values.data());
}

size_t Int64ColumnWriter::get_size() {
    return m_values.size_in_bytes();
}

void FloatColumnWriter::add_value(ordered_json* value) {
    m_values.push_back(value->get<float>());
    // m_values.push_back(value.d);
}

char* FloatColumnWriter::get_data() {
    return reinterpret_cast<char*>(m_values.data());
}

size_t FloatColumnWriter::get_size() {
    return m_values.size_in_bytes();
}

void StringColumnWriter::add_value(ordered_json* value) {
    std::string string_var = value->get<std::string>();
    variable_dictionary_id_t id;
    m_var_dict.add_occurrence(string_var, id);
    m_length += string_var.length();
    m_values.push_back(id);
}

char* StringColumnWriter::get_data() {
    // std::string final;

    // for (auto i: m_values) {
    //     size_t length = i.length();
    //     final += std::string(static_cast<char*>(static_cast<void*>(&length)), sizeof(size_t));
    //     final += i;
    // }

    // m_ptr = new char[final.size() + 1];
    // strcpy(m_ptr, final.c_str());
    // m_length = final.length();

    // return m_ptr;
    return reinterpret_cast<char*>(m_values.data());
}

size_t StringColumnWriter::get_size() {
    // return m_length;
    return m_values.size_in_bytes();
}

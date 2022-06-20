#ifndef COLUMNWRITER_HPP
#define COLUMNWRITER_HPP

#include "../submodules/json/single_include/nlohmann/json.hpp"
#include "PageAllocatedVector.hpp"
#include "VariableDictionaryWriter.hpp"
#include "Utils.hpp"

using nlohmann::ordered_json;
class BaseColumnWriter {
public:
    BaseColumnWriter(std::string name): m_name(name) {}
    virtual ~BaseColumnWriter() = default;
    virtual void add_value(ordered_json* value) = 0;
    virtual char* get_data() = 0;
    virtual size_t get_size() = 0;
    std::string get_name() { return m_name; }
private:
    std::string m_name;    
};

class Int64ColumnWriter: public BaseColumnWriter {
public:
    Int64ColumnWriter(std::string name): BaseColumnWriter(name) {}
    ~Int64ColumnWriter() = default;
    void add_value(ordered_json* value) override;
    char* get_data() override;
    size_t get_size() override;
private:
    PageAllocatedVector<int64_t> m_values;
};

class FloatColumnWriter: public BaseColumnWriter {
public:
    FloatColumnWriter(std::string name): BaseColumnWriter(name) {}
    ~FloatColumnWriter() = default;
    void add_value(ordered_json* value) override;
    char* get_data() override;
    size_t get_size() override;
private:
    PageAllocatedVector<float> m_values;
};

class StringColumnWriter: public BaseColumnWriter {
public:
    // StringColumnWriter(std::string name): BaseColumnWriter(name), m_ptr(NULL), m_length(0UL) {}
    // ~StringColumnWriter() {
    //     if (m_ptr != NULL) {
    //         delete m_ptr;
    //     }
    // }

    StringColumnWriter(std::string name, std::string dir): BaseColumnWriter(name), m_length(0UL) {
        std::string dict_path = dir + name + ".dict";
        std::string index_path = dir + name + ".index";
        m_var_dict.open(dict_path, index_path, cVariableDictionaryIdMax);
    }

    ~StringColumnWriter() {
        m_var_dict.close();
    }

    // void add_value(ordered_json* value) override;
    void add_value(ordered_json* value) override;
    char* get_data() override;
    size_t get_size() override;
private:    
    // std::vector<std::string> m_values;
    // char* m_ptr;
    VariableDictionaryWriter m_var_dict;
    size_t m_length;
    PageAllocatedVector<encoded_variable_t> m_values;
};

#endif // COLUMNWRITER_HPP
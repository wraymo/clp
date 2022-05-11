#ifndef COLUMNWRITER_HPP
#define COLUMNWRITER_HPP

#include "../submodules/json/single_include/nlohmann/json.hpp"
#include "PageAllocatedVector.hpp"

using nlohmann::ordered_json;
class BaseColumnWriter {
public:
    virtual ~BaseColumnWriter() = default;
    virtual void add_value(ordered_json* value) = 0;
    virtual char* get_data() = 0;
    virtual size_t get_size() = 0;
};

class Int64ColumnWriter: public BaseColumnWriter {
public:
    ~Int64ColumnWriter() = default;
    void add_value(ordered_json* value) override;
    char* get_data() override;
    size_t get_size() override;
private:
    PageAllocatedVector<int64_t> m_values;
};

class FloatColumnWriter: public BaseColumnWriter {
public:
    ~FloatColumnWriter() = default;
    void add_value(ordered_json* value) override;
    char* get_data() override;
    size_t get_size() override;
private:
    PageAllocatedVector<float> m_values;
};

class StringColumnWriter: public BaseColumnWriter {
public:
    StringColumnWriter(): m_ptr(NULL), m_length(0UL) {}
    ~StringColumnWriter() {
        if (m_ptr != NULL) {
            delete m_ptr;
        }
    }

    void add_value(ordered_json* value) override;
    char* get_data() override;
    size_t get_size() override;
private:    
    std::vector<std::string> m_values;
    char* m_ptr;
    size_t m_length;
};

#endif // COLUMNWRITER_HPP
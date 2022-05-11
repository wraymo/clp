#ifndef PARSEDMESSAGE_HPP
#define PARSEDMESSAGE_HPP

// C++ standard libraries
#include <string>

// json
#include "../submodules/json/single_include/nlohmann/json.hpp"

// Project headers
#include "TimestampPattern.hpp"

using nlohmann::ordered_json;

/**
 * ParsedMessage represents a (potentially multiline) log message parsed into 3 primary fields: timestamp, timestamp pattern, and content.
 */
class ParsedMessage {
public:
    enum class MessageType { TEXT, JSON };
    // Constructors
    ParsedMessage () : m_content({}), m_message_type(MessageType::TEXT), m_ts_patt(nullptr), m_ts_patt_changed(false), m_ts(0),
                       m_orig_num_bytes(0), m_is_set(false) {}

    // Disable copy and move constructor/assignment
    ParsedMessage (const ParsedMessage&) = delete;
    ParsedMessage& operator= (const ParsedMessage&) = delete;

    // Destructors
    ~ParsedMessage () {
        for (auto i: m_extracted_values) 
            if (i != nullptr)
                delete i;
    }

    // Methods
    void clear ();
    void clear_except_ts_patt ();

    void set (const TimestampPattern* timestamp_pattern, epochtime_t timestamp, const std::string& line, size_t timestamp_begin_pos, size_t timestamp_end_pos);
    void set (const TimestampPattern* timestamp_pattern, epochtime_t timestamp, const ordered_json& line);
    void append_line (const std::string& line);

    /**
     * Move all data from the given message into the current message while clearing the given message
     * @param message
     */
    void consume (ParsedMessage& message);

    const std::string& get_content () const { return m_content; }
    ordered_json& get_json_content () { return m_json_content; }
    MessageType get_message_type() const { return m_message_type; }
    size_t get_orig_num_bytes () const { return m_orig_num_bytes; }
    epochtime_t get_ts () const { return m_ts; }
    const TimestampPattern* get_ts_patt () const { return m_ts_patt; }
    bool has_ts_patt_changed () const { return m_ts_patt_changed; }
    bool is_empty () const { return false == m_is_set; }

    void add_extracted_value (ordered_json* value) { m_extracted_values.push_back(value); }
    std::vector<ordered_json*> get_extracted_values () const { return m_extracted_values; }

private:
    // Variables
    std::string m_content;
    ordered_json m_json_content;
    std::vector<ordered_json*> m_extracted_values; 
    MessageType m_message_type;
    const TimestampPattern* m_ts_patt;
    bool m_ts_patt_changed;
    epochtime_t m_ts;
    size_t m_orig_num_bytes;
    bool m_is_set;
};

#endif // PARSEDMESSAGE_HPP

#include "MessageParser.hpp"

// Project headers
#include "Defs.h"
#include "TimestampPattern.hpp"

#include <chrono>
#include <iostream>

// Constants
constexpr char cLineDelimiter = '\n';
constexpr int cNumTimestampNames = 7;
const char* cTimestampNames[] = {"Time", "TIME", "time", "timestamp", "Timestamp", "TimeStamp", "TIMESTAMP"};

bool MessageParser::parse_next_message (bool drain_source, size_t buffer_length, const char* buffer, size_t& buf_pos, ParsedMessage& message) {
    message.clear_except_ts_patt();

    while (true) {
        // Check if the buffer was exhausted
        if (buffer_length == buf_pos) {
            break;
        }

        // Read a line up to the delimiter
        bool found_delim = false;

        if (false == m_line.empty() && m_line.back() == cLineDelimiter) {
            found_delim = true;
        } else {
            for (; false == found_delim && buf_pos < buffer_length; ++buf_pos) {
                auto c = buffer[buf_pos];

                m_line += c;
                if (cLineDelimiter == c) {
                    found_delim = true;
                }
            }
        }

        if (false == found_delim && false == drain_source) {
            // No delimiter was found and the source doesn't need to be drained
            return false;
        }

        if (parse_line(message)) {
            return true;
        }
    }

    return false;
}

bool MessageParser::parse_next_message (bool drain_source, ReaderInterface& reader, ParsedMessage& message) {
    message.clear_except_ts_patt();

    while (true) {
        // Read message
        if (true == m_line.empty() || m_line.back() != cLineDelimiter) {
            auto error_code = reader.try_read_to_delimiter(cLineDelimiter, true, true, m_line);
            if (ErrorCode_Success != error_code) {
                if (ErrorCode_EndOfFile != error_code) {
                    throw OperationFailed(error_code, __FILENAME__, __LINE__);
                }
                if (m_line.empty()) {
                    if (m_buffered_msg.is_empty()) {
                        break;
                    } else {
                        message.consume(m_buffered_msg);
                        return true;
                    }
                }
            }
        }

        if (false == drain_source && cLineDelimiter != m_line[m_line.length() - 1]) {
            return false;
        }

        if (parse_line(message)) {
            return true;
        }
    }

    return false;
}

bool MessageParser::parse_next_json_message(ReaderInterface &reader, size_t buffer_length, const char* buffer, size_t& buf_pos,
                                            ParsedMessage& message) {
    if (buf_pos >= buffer_length)
        return false;

    bool found_delim = false;
    for (; false == found_delim && buf_pos < buffer_length; ++buf_pos) {
        auto c = buffer[buf_pos];

        m_line += c;
        if (cLineDelimiter == c) {
            found_delim = true;
        }
    }

    if (false == found_delim) {
        return parse_next_json_message(reader, message);
    }

    return parse_json_line(message);
}

bool MessageParser::parse_next_json_message(ReaderInterface &reader, ParsedMessage& message) {
    message.clear_except_ts_patt();

    auto error_code = reader.try_read_to_delimiter(cLineDelimiter, true, true, m_line);
    if (ErrorCode_Success != error_code) {
        if (ErrorCode_EndOfFile != error_code) {
            throw OperationFailed(error_code, __FILENAME__, __LINE__);
        }
        return false;
    }

    return parse_json_line(message);
}

/**
 * The general algorithm is as follows:
 * - Try to parse a timestamp from the line.
 * - If the line has a timestamp and...
 *   - ...the buffered message is empty, fill it and continue reading.
 *   - ...the buffered message is not empty, save the line for the next message and return the buffered message.
 * - Else if the line has no timestamp and...
 *   - ...the buffered message is empty, return the line as a message.
 *   - ...the buffered message is not empty, add the line to the message and continue reading.
 */
bool MessageParser::parse_line (ParsedMessage& message) {
    bool message_completed = false;

    // Parse timestamp and content
    const TimestampPattern* timestamp_pattern = message.get_ts_patt();
    epochtime_t timestamp = 0;
    size_t timestamp_begin_pos;
    size_t timestamp_end_pos;
    if (nullptr == timestamp_pattern || false == timestamp_pattern->parse_timestamp(m_line, timestamp, timestamp_begin_pos, timestamp_end_pos)) {
        timestamp_pattern = TimestampPattern::search_known_ts_patterns(m_line, timestamp, timestamp_begin_pos, timestamp_end_pos);
    }

    if (nullptr != timestamp_pattern) {
        // A timestamp was parsed
        if (m_buffered_msg.is_empty()) {
            // Fill message with line
            m_buffered_msg.set(timestamp_pattern, timestamp, m_line, timestamp_begin_pos, timestamp_end_pos);
        } else {
            // Move buffered message to message
            message.consume(m_buffered_msg);

            // Save line for next message
            m_buffered_msg.set(timestamp_pattern, timestamp, m_line, timestamp_begin_pos, timestamp_end_pos);
            message_completed = true;
        }
    } else {
        // No timestamp was parsed
        if (m_buffered_msg.is_empty()) {
            // Fill message with line
            message.set(timestamp_pattern, timestamp, m_line, timestamp_begin_pos, timestamp_end_pos);
            message_completed = true;
        } else {
            // Append line to message
            m_buffered_msg.append_line(m_line);
        }
    }

    m_line.clear();
    return message_completed;
}

bool MessageParser::parse_json_line(ParsedMessage& message) {
//    auto begin = std::chrono::system_clock::now();
    ordered_json object = ordered_json::parse(m_line, nullptr, false);
//    auto end = std::chrono::system_clock::now();
//    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - begin);
//    std::cout << "MessageParser::parse_json_line parse time: " << duration.count() << std::endl;

    if (object.is_discarded()) {
        return false;
    }

    size_t pos;
    for (pos = 0; pos < cNumTimestampNames; ++pos) {
        if (object.find(cTimestampNames[pos]) != object.end()) {
            break;
        }
    }

    if (pos < cNumTimestampNames) {
        const TimestampPattern* timestamp_pattern = message.get_ts_patt();
        epochtime_t timestamp = 0;
        size_t timestamp_begin_pos;
        size_t timestamp_end_pos;

        if (object[cTimestampNames[pos]].is_number_integer()) {
            message.set(nullptr, object[cTimestampNames[pos]].get<epochtime_t>(), object);
        } else {
            if (object[cTimestampNames[pos]].is_string()) {
                std::string parsed_string = object[cTimestampNames[pos]].get<std::string>();
                if (nullptr == timestamp_pattern ||
                    false == timestamp_pattern->parse_timestamp(parsed_string, timestamp,timestamp_begin_pos, timestamp_end_pos)) {
                    timestamp_pattern = TimestampPattern::search_known_ts_patterns(parsed_string, timestamp, timestamp_begin_pos, timestamp_end_pos);
                }

                message.set(timestamp_pattern, timestamp, object);
            }
        }
    } else {
        message.set(nullptr, 0, object);
    }

    m_line.clear();
    return true;
}
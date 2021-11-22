#ifndef STREAMING_ARCHIVE_READER_MESSAGE_HPP
#define STREAMING_ARCHIVE_READER_MESSAGE_HPP

// C++ libraries
#include <vector>

// Project headers
#include "../../Defs.h"

namespace streaming_archive { namespace reader {
    class Message {
    public:
        enum class MessageType { TEXT, JSON };
        // Methods
        size_t get_message_number () const;
        MessageType get_message_type () const;
        logtype_dictionary_id_t get_logtype_id () const;
        jsontype_dictionary_id_t get_jsontype_id () const;
        const std::vector<encoded_variable_t>& get_vars () const;
        epochtime_t get_ts_in_milli () const;

        void set_message_number (uint64_t message_number);
        void set_message_type (MessageType message_type);
        void set_logtype_id (logtype_dictionary_id_t logtype_id);
        void set_jsontype_id (jsontype_dictionary_id_t jsontype_id);
        void add_var (encoded_variable_t var);
        void set_timestamp (epochtime_t timestamp);

        void clear_vars ();

    private:
        friend class Archive;

        // Variables
        size_t m_message_number;
        MessageType m_message_type;
        logtype_dictionary_id_t m_logtype_id;
        logtype_dictionary_id_t m_jsontype_id;
        std::vector<encoded_variable_t> m_vars;
        epochtime_t m_timestamp;
    };
} }

#endif // STREAMING_ARCHIVE_READER_MESSAGE_HPP

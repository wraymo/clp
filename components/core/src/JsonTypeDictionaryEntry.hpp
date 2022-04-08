#ifndef JSONTYPEDICTIONARYENTRY_HPP
#define JSONTYPEDICTIONARYENTRY_HPP

// C++ standard libraries
#include <vector>

// json
#include "../submodules/json/single_include/nlohmann/json.hpp"

// Project headers
#include "Defs.h"
#include "DictionaryEntry.hpp"
#include "ErrorCode.hpp"
#include "FileReader.hpp"
#include "streaming_compression/zstd/Compressor.hpp"
#include "streaming_compression/zstd/Decompressor.hpp"
#include "TraceableException.hpp"

using nlohmann::ordered_json;

/**
 * Class representing a jsontype dictionary entry
 */
class JsonTypeDictionaryEntry : public DictionaryEntry<jsontype_dictionary_id_t> {
public:
    // Types
    class OperationFailed : public TraceableException {
    public:
        // Constructors
        OperationFailed (ErrorCode error_code, const char* const filename, int line_number) : TraceableException (error_code, filename, line_number) {}

        // Methods
        const char* what () const noexcept override {
            return "JsonTypeDictionaryEntry operation failed";
        }
    };

    // Constants
    enum class Delim {
        // NOTE: These values are used within logtypes to denote variables, so care must be taken when changing them
        NonDoubleVar = 17,
        DoubleVar = 18,
        LogType = 19,
        StringVar = 20,
        BooleanVar = 21,
    };

    static constexpr size_t cMaxDigitsInRepresentableDoubleVar = 15;

    // Constructors
    JsonTypeDictionaryEntry () = default;
    // Use default copy constructor
    JsonTypeDictionaryEntry (const JsonTypeDictionaryEntry&) = default;

    // Assignment operators
    // Use default
    JsonTypeDictionaryEntry& operator= (const JsonTypeDictionaryEntry&) = default;

    // Methods
    static void add_logtype(ordered_json& value);
    static void add_logtype (ordered_json& value, logtype_dictionary_id_t& id);
    static void add_string_var (ordered_json& value);
    static void add_non_double_var (ordered_json& value);
    static void add_double_var (ordered_json& value, uint8_t num_integer_digits, uint8_t num_fractional_digits);
    static void add_boolean_var(ordered_json& value);

    static bool is_string_var (std::string& str);
    static bool is_non_double_var (std::string& str);
    static bool is_double_var (std::string &str);
    static bool is_boolean_var (std::string &str);
    static bool is_logtype (std::string& str);

    static logtype_dictionary_id_t get_logtype_id (std::string& str);

    void set (ordered_json& object);

    /**
     * Gets the size (in-memory) of the data contained in this entry
     * @return Size of the data contained in this entry
     */
    size_t get_data_size () const;


    void set_id (jsontype_dictionary_id_t id) { m_id = id; }

    /**
     * Writes an entry to file
     * @param compressor
     */
    void write_to_file (streaming_compression::zstd::Compressor& compressor) const;
    /**
     * Tries to read an entry from the given decompressor
     * @param decompressor
     * @return Same as streaming_compression::zstd::Decompressor::try_read_numeric_value
     * @return Same as streaming_compression::zstd::Decompressor::try_read_string
     */
    ErrorCode try_read_from_file (streaming_compression::zstd::Decompressor& decompressor);
    /**
     * Reads an entry from the given decompressor
     * @param decompressor
     */
    void read_from_file (streaming_compression::zstd::Decompressor& decompressor);

    size_t get_num_vars() const;
    void set_num_vars(size_t num_vars);

private:
    size_t m_num_vars = 0;
};

#endif //JSONTYPEDICTIONARYENTRY_HPP

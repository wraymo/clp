#ifndef JSONTYPEDICTIONARYWRITER_HPP
#define JSONTYPEDICTIONARYWRITER_HPP

// C++ standard libraries
#include <memory>

// Project headers
#include "Defs.h"
#include "DictionaryWriter.hpp"
#include "FileWriter.hpp"
#include "JsonTypeDictionaryEntry.hpp"

/**
 * Class for performing operations on logtype dictionaries and writing them to disk
 */
class JsonTypeDictionaryWriter : public DictionaryWriter<jsontype_dictionary_id_t, JsonTypeDictionaryEntry> {
public:
    // Types
    class OperationFailed : public TraceableException {
    public:
        // Constructors
        OperationFailed (ErrorCode error_code, const char* const filename, int line_number) : TraceableException (error_code, filename, line_number) {}

        // Methods
        const char* what () const noexcept override {
            return "JsonTypeDictionaryWriter operation failed";
        }
    };

    bool add_occurrence (std::unique_ptr<JsonTypeDictionaryEntry>& entry_wrapper, jsontype_dictionary_id_t& jsontype_id);
};



#endif //JSONTYPEDICTIONARYWRITER_HPP

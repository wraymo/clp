#ifndef CLP_JSONTYPEDICTIONARYREADER_HPP
#define CLP_JSONTYPEDICTIONARYREADER_HPP

// Project headers
#include "Defs.h"
#include "DictionaryReader.hpp"
#include "JsonTypeDictionaryEntry.hpp"

/**
 * Class for reading jsontype dictionaries from disk and performing operations on them
 */
class JsonTypeDictionaryReader : public DictionaryReader<jsontype_dictionary_id_t, JsonTypeDictionaryEntry> {

};

#endif //CLP_JSONTYPEDICTIONARYREADER_HPP

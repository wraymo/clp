//
// Created by Rui on 2021/11/8.
//

#include "JsonTypeDictionaryWriter.hpp"

using std::string;

bool JsonTypeDictionaryWriter::add_occurrence(std::unique_ptr<JsonTypeDictionaryEntry> &entry_wrapper,
                                              jsontype_dictionary_id_t &jsontype_id) {
    if (nullptr == entry_wrapper) {
        throw OperationFailed(ErrorCode_BadParam, __FILENAME__, __LINE__);
    }
    auto& entry = *entry_wrapper;

    bool is_new_entry = false;

    const string& value = entry.get_value();
    const auto ix = m_value_to_entry.find(value);
    if (m_value_to_entry.end() != ix) {
        // Entry exists so increment its count
        auto& existing_entry = ix->second;
        jsontype_id = existing_entry->get_id();
    } else {
        // Assign ID
        jsontype_id = m_next_id;
        ++m_next_id;
        entry.set_id(jsontype_id);

        // Insert new entry into dictionary
        auto entry_ptr = entry_wrapper.release();
        m_value_to_entry[value] = entry_ptr;
        m_id_to_entry[jsontype_id] = entry_ptr;

        // Mark ID as dirty
        m_uncommitted_entries.emplace_back(entry_ptr);

        is_new_entry = true;

        m_data_size += entry_ptr->get_data_size();
    }
    return is_new_entry;
}


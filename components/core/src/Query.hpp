#ifndef QUERY_HPP
#define QUERY_HPP

// C++ standard libraries
#include <set>
#include <string>
#include <unordered_set>
#include <vector>

// Project headers
#include "Defs.h"
#include "LogTypeDictionaryEntry.hpp"
#include "VariableDictionaryEntry.hpp"

/**
 * Class representing a variable in a subquery. It can represent a precise encoded variable or an imprecise dictionary variable (i.e., a set of possible
 * encoded dictionary variable IDs)
 */
class QueryVar {
public:
    // Constructors
    explicit QueryVar (encoded_variable_t precise_non_dict_var);
    QueryVar (encoded_variable_t precise_dict_var, const VariableDictionaryEntry* var_dict_entry);
    QueryVar (const std::unordered_set<encoded_variable_t>& possible_dict_vars,
              const std::unordered_set<const VariableDictionaryEntry*>& possible_var_dict_entries);

    // Methods
    /**
     * Checks if the given encoded variable matches this QueryVar
     * @param var
     * @return true if matched, false otherwise
     */
    bool matches (encoded_variable_t var) const;

    /**
     * Removes segments from the given set that don't contain the given variable
     * @param segment_ids
     */
    void remove_segments_that_dont_contain_dict_var (std::set<segment_id_t>& segment_ids) const;

    bool is_precise_var () const { return m_is_precise_var; }
    bool is_dict_var () const { return m_is_dict_var; }
    const VariableDictionaryEntry* get_var_dict_entry () const { return m_var_dict_entry; }
    const std::unordered_set<const VariableDictionaryEntry*>& get_possible_var_dict_entries () const { return m_possible_var_dict_entries; }

private:
    // Variables
    bool m_is_precise_var;
    bool m_is_dict_var;

    encoded_variable_t m_precise_var;
    // Only used if the precise variable is a dictionary variable
    const VariableDictionaryEntry* m_var_dict_entry;

    // Only used if the variable is an imprecise dictionary variable
    std::unordered_set<encoded_variable_t> m_possible_dict_vars;
    std::unordered_set<const VariableDictionaryEntry*> m_possible_var_dict_entries;
};

/**
 * Class representing a subquery (or informally, an interpretation) of a user query. It contains a series of possible logtypes, a set of QueryVars, and whether
 * the query still requires wildcard matching after it matches an encoded message.
 */
class SubQuery {
public:
    // Methods
    /**
     * Adds a precise non-dictionary variable to the subquery
     * @param precise_non_dict_var
     */
    void add_non_dict_var (encoded_variable_t precise_non_dict_var);
    /**
     * Adds a precise dictionary variable to the subquery
     * @param precise_dict_var
     * @param var_dict_entry
     */
    void add_dict_var (encoded_variable_t precise_dict_var, const VariableDictionaryEntry* var_dict_entry);
    /**
     * Adds an imprecise dictionary variable (i.e., a set of possible precise dictionary variables) to the subquery
     * @param possible_dict_vars
     * @param possible_var_dict_entries
     */
    void add_imprecise_dict_var (const std::unordered_set<encoded_variable_t>& possible_dict_vars,
                                 const std::unordered_set<const VariableDictionaryEntry*>& possible_var_dict_entries);
    /**
     * Add a set of possible logtypes to the subquery
     * @param logtype_entries
     */
    void set_possible_logtypes (const std::unordered_set<const LogTypeDictionaryEntry*>& logtype_entries);
    void mark_wildcard_match_required ();

    /**
     * Calculates the segment IDs that should contain a match for the subquery's current logtypes and QueryVars
     */
    void calculate_ids_of_matching_segments ();

    void clear ();

    bool wildcard_match_required () const { return m_wildcard_match_required; }
    size_t get_num_possible_logtypes () const { return m_possible_logtype_ids.size(); }
    const std::unordered_set<const LogTypeDictionaryEntry*>& get_possible_logtype_entries () const { return m_possible_logtype_entries; };
    size_t get_num_possible_vars () const { return m_vars.size(); }
    const std::vector<QueryVar>& get_vars () const { return m_vars; }
    const std::set<segment_id_t>& get_ids_of_matching_segments () const { return m_ids_of_matching_segments; }

    /**
     * Whether the given logtype ID matches one of the possible logtypes in this subquery
     * @param logtype
     * @return true if matched, false otherwise
     */
    bool matches_logtype (logtype_dictionary_id_t logtype) const;
    /**
     * Whether the given variables contain the subquery's variables in order (but not necessarily contiguously)
     * @param vars
     * @return true if matched, false otherwise
     */
    bool matches_vars (const std::vector<encoded_variable_t>& vars) const;

private:
    // Variables
    std::unordered_set<const LogTypeDictionaryEntry*> m_possible_logtype_entries;
    std::unordered_set<logtype_dictionary_id_t> m_possible_logtype_ids;
    std::set<segment_id_t> m_ids_of_matching_segments;
    std::vector<QueryVar> m_vars;
    bool m_wildcard_match_required;
};

/**
 * Class representing a user query with potentially multiple sub-queries.
 */
class Query {
public:
    // Constructors
    Query () : m_search_begin_timestamp(cEpochTimeMin), m_search_end_timestamp(cEpochTimeMax), m_ignore_case(false), m_search_string_matches_all(true),
            m_all_subqueries_relevant(true) {}

    // Methods
    void set_search_begin_timestamp (epochtime_t timestamp) { m_search_begin_timestamp = timestamp; }
    void set_search_end_timestamp (epochtime_t timestamp) { m_search_end_timestamp = timestamp; }
    void set_ignore_case (bool ignore_case) { m_ignore_case = ignore_case; }
    void set_search_string (const std::string& search_string);
    void add_sub_query (const SubQuery& sub_query);
    void clear_sub_queries ();
    /**
     * Populates the set of relevant sub-queries with all possible sub-queries from the query
     */
    void make_all_sub_queries_relevant ();
    /**
     * Populates the set of relevant sub-queries with only those that match the given segment
     * @param segment_id
     */
    void make_sub_queries_relevant_to_segment (segment_id_t segment_id);

    epochtime_t get_search_begin_timestamp () const { return m_search_begin_timestamp; }
    epochtime_t get_search_end_timestamp () const { return m_search_end_timestamp; }
    /**
     * Checks if the given timestamp is in the search time range (begin and end inclusive)
     * @param timestamp
     * @return true if the timestamp is in the search time range
     * @return false otherwise
     */
    bool timestamp_is_in_search_time_range (epochtime_t timestamp) const {
        return (m_search_begin_timestamp <= timestamp && timestamp <= m_search_end_timestamp);
    }
    bool get_ignore_case () const { return m_ignore_case; }
    const std::string& get_search_string () const { return m_search_string; }
    /**
     * Checks if the search string will match all messages (i.e., it's "" or "*")
     * @return true if the search string will match all messages
     * @return false otherwise
     */
    bool search_string_matches_all () const { return m_search_string_matches_all; }
    const std::vector<SubQuery>& get_sub_queries () const { return m_sub_queries; }
    bool contains_sub_queries () const { return m_sub_queries.empty() == false; }
    const std::vector<const SubQuery*>& get_relevant_sub_queries () const { return m_relevant_sub_queries; }

private:
    // Variables
    // Start of search time range (inclusive)
    epochtime_t m_search_begin_timestamp;
    // End of search time range (inclusive)
    epochtime_t m_search_end_timestamp;
    bool m_ignore_case;
    std::string m_search_string;
    bool m_search_string_matches_all;
    std::vector<SubQuery> m_sub_queries;
    std::vector<const SubQuery*> m_relevant_sub_queries;
    segment_id_t m_prev_segment_id;
    bool m_all_subqueries_relevant;
};


#endif // QUERY_HPP

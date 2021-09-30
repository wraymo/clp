#ifndef GREP_HPP
#define GREP_HPP

// C++ libraries
#include <string>

// Project headers
#include "Defs.h"
#include "Query.hpp"
#include "streaming_archive/reader/Archive.hpp"
#include "streaming_archive/reader/File.hpp"

class Grep {
public:
    // Types
    /**
     * Handles search result
     * @param orig_file_path Path of uncompressed file
     * @param compressed_msg
     * @param decompressed_msg
     * @param custom_arg Custom argument for the output function
     */
    typedef void (*OutputFunc) (const std::string& orig_file_path, const streaming_archive::reader::Message& compressed_msg,
            const std::string& decompressed_msg, void* custom_arg);

    // Methods
    /**
     * Processes a raw user query into a Query
     * @param archive
     * @param search_string
     * @param search_begin_ts
     * @param search_end_ts
     * @param ignore_case
     * @param query
     * @return true if query may match messages, false otherwise
     */
    static bool process_raw_query (const streaming_archive::reader::Archive& archive, const std::string& search_string,
                                   epochtime_t search_begin_ts, epochtime_t search_end_ts, bool ignore_case, Query& query);

    /**
     * Marks which sub-queries in each query are relevant to the given file
     * @param compressed_file
     * @param queries
     */
    static void calculate_sub_queries_relevant_to_file (const streaming_archive::reader::File& compressed_file, std::vector<Query>& queries);

    /**
     * Searches a file with the given query and outputs any results using the given method
     * @param query
     * @param limit
     * @param archive
     * @param compressed_file
     * @param output_func
     * @param output_func_arg
     * @return Number of matches found
     * @throw streaming_archive::reader::Archive::OperationFailed if decompression unexpectedly fails
     * @throw TimestampPattern::OperationFailed if failed to insert timestamp into message
     */
    static size_t search_and_output (const Query& query, size_t limit, streaming_archive::reader::Archive& archive,
                                     streaming_archive::reader::File& compressed_file, OutputFunc output_func, void* output_func_arg);
    static bool search_and_decompress (const Query& query, streaming_archive::reader::Archive& archive, streaming_archive::reader::File& compressed_file,
            streaming_archive::reader::Message& compressed_msg, std::string& decompressed_msg);
    /**
     * Searches a file with the given query without outputting the results
     * @param query
     * @param limit
     * @param archive
     * @param compressed_file
     * @return Number of matches found
     * @throw streaming_archive::reader::Archive::OperationFailed if decompression unexpectedly fails
     * @throw TimestampPattern::OperationFailed if failed to insert timestamp into message
     */
    static size_t search (const Query& query, size_t limit, streaming_archive::reader::Archive& archive, streaming_archive::reader::File& compressed_file);
};

#endif // GREP_HPP

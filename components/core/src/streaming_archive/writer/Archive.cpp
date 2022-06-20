#include "Archive.hpp"

// C libraries
#include <sys/stat.h>

// C++ libraries
#include <iostream>
#include <fstream>

// Boost libraries
#include <boost/asio.hpp>
#include <boost/filesystem.hpp>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>

// spdlog
#include <spdlog/spdlog.h>

// Project headers
#include "../../EncodedVariableInterpreter.hpp"
#include "../../Profiler.hpp"
#include "../../Utils.hpp"
#include "../Constants.hpp"

using std::list;
using std::make_unique;
using std::string;
using std::unordered_set;
using std::vector;
using nlohmann::ordered_json;

namespace streaming_archive { namespace writer {
    Archive::~Archive () {
        if (m_path.empty() == false || m_mutable_files.empty() == false || m_files_with_timestamps_in_segment.empty() == false ||
                m_files_without_timestamps_in_segment.empty() == false)
        {
            SPDLOG_ERROR("Archive not closed before being destroyed - data loss may occur");
            for (auto file : m_mutable_files) {
                delete file;
            }
            for (auto file : m_files_with_timestamps_in_segment) {
                delete file;
            }
            for (auto file : m_files_without_timestamps_in_segment) {
                delete file;
            }
            for (size_t i = 0; i < m_segments_for_columns.size(); i++) {
                if (m_segments_for_columns[i] != nullptr) {
                    delete m_segments_for_columns[i];
                }
            }
        }
    }

    void Archive::parse_keys (std::map<std::string, std::string> keys) {
        for (auto &pair: keys) {
            m_preparsed_keys_original.push_back(pair.first);

            std::vector<std::string> temp;
            size_t i;
            auto key = pair.first;
            while ((i = key.find(".")) != std::string::npos) {
                temp.push_back(key.substr(0, i));
                key = key.substr(i + 1);
            }
            if (key != "") 
                temp.push_back(key);
            m_preparsed_keys[temp] = pair.second;

            Segment* segment = new Segment();
            m_segments_for_columns.push_back(segment);
        }
    }

    void Archive::open (const UserConfig& user_config) {
        int retval;

        m_id = user_config.id;
        m_id_as_string = boost::uuids::to_string(m_id);
        m_creator_id = user_config.creator_id;
        m_creator_id_as_string = boost::uuids::to_string(m_creator_id);
        m_creation_num = user_config.creation_num;
        m_print_archive_stats_progress = user_config.print_archive_stats_progress;

        parse_keys(user_config.preparsed_keys);

        boost::system::error_code boost_error_code;

        // Ensure path doesn't already exist
        auto archive_path = boost::filesystem::path(user_config.output_dir) / m_id_as_string;
        bool path_exists = boost::filesystem::exists(archive_path, boost_error_code);
        if (path_exists) {
            SPDLOG_ERROR("Archive path already exists: {}", archive_path.c_str());
            throw OperationFailed(ErrorCode_Unsupported, __FILENAME__, __LINE__);
        }
        const auto& archive_path_string = archive_path.string();
        m_stable_uncompressed_size = 0;
        m_stable_size = 0;

        // Create internal directories if necessary
        retval = mkdir(archive_path_string.c_str(), 0750);
        if (0 != retval) {
            SPDLOG_ERROR("Failed to create {}, errno={}", archive_path_string.c_str(), errno);
            throw OperationFailed(ErrorCode_errno, __FILENAME__, __LINE__);
        }

        // Get archive directory's file descriptor
        int archive_dir_fd = ::open(archive_path_string.c_str(), O_RDONLY);
        if (-1 == archive_dir_fd) {
            SPDLOG_ERROR("Failed to get file descriptor for {}, errno={}", archive_path_string.c_str(), errno);
            throw OperationFailed(ErrorCode_errno, __FILENAME__, __LINE__);
        }

        // Create logs directory
        m_logs_dir_path = archive_path_string;
        m_logs_dir_path += '/';
        m_logs_dir_path += cLogsDirname;
        m_logs_dir_path += '/';
        retval = mkdir(m_logs_dir_path.c_str(), 0750);
        if (0 != retval) {
            SPDLOG_ERROR("Failed to create {}, errno={}", m_logs_dir_path.c_str(), errno);
            throw OperationFailed(ErrorCode_errno, __FILENAME__, __LINE__);
        }

        // Get logs directory's file descriptor
        m_logs_dir_fd = ::open(m_logs_dir_path.c_str(), O_RDONLY);
        if (-1 == m_logs_dir_fd) {
            SPDLOG_ERROR("Failed to open file descriptor for {}, errno={}", m_logs_dir_path.c_str(), errno);
            throw OperationFailed(ErrorCode_errno, __FILENAME__, __LINE__);
        }

        // Create segments directory
        m_segments_dir_path = archive_path_string;
        m_segments_dir_path += '/';
        m_segments_dir_path += cSegmentsDirname;
        m_segments_dir_path += '/';
        retval = mkdir(m_segments_dir_path.c_str(), 0750);
        if (0 != retval) {
            SPDLOG_ERROR("Failed to create {}, errno={}", m_segments_dir_path.c_str(), errno);
            throw OperationFailed(ErrorCode_errno, __FILENAME__, __LINE__);
        }

        // Create column segment directory
        m_column_segments_dir_path = archive_path_string;
        m_column_segments_dir_path += '/';
        m_column_segments_dir_path += cColumnSegmentsDirname;
        m_column_segments_dir_path += '/';
        retval = mkdir(m_column_segments_dir_path.c_str(), 0750);
        if (0 != retval) {
            SPDLOG_ERROR("Failed to create {}, errno={}", m_column_segments_dir_path.c_str(), errno);
            throw OperationFailed(ErrorCode_errno, __FILENAME__, __LINE__);
        }


        // Get segments directory's file descriptor
        m_segments_dir_fd = ::open(m_segments_dir_path.c_str(), O_RDONLY);
        if (-1 == m_segments_dir_fd) {
            SPDLOG_ERROR("Failed to open file descriptor for {}, errno={}", m_segments_dir_path.c_str(), errno);
            throw OperationFailed(ErrorCode_errno, __FILENAME__, __LINE__);
        }

        // Create metadata database
        auto metadata_db_path = archive_path / cMetadataDBFileName;
        m_metadata_db.open(metadata_db_path.string());

        m_next_file_id = 0;

        m_target_segment_uncompressed_size = user_config.target_segment_uncompressed_size;
        m_next_segment_id = 0;
        m_compression_level = user_config.compression_level;

        // Save metadata to disk
        auto metadata_file_path = archive_path / cMetadataFileName;
        try {
            m_metadata_file_writer.open(metadata_file_path.string(), FileWriter::OpenMode::CREATE_IF_NONEXISTENT_FOR_SEEKABLE_WRITING);
            // Update size before we write the metadata file so we can store the size in the metadata file
            m_stable_size += sizeof(cArchiveFormatVersion) + sizeof(m_stable_uncompressed_size) + sizeof(m_stable_size);

            m_metadata_file_writer.write_numeric_value(cArchiveFormatVersion);
            m_metadata_file_writer.write_numeric_value(m_stable_uncompressed_size);
            m_metadata_file_writer.write_numeric_value(m_stable_size);
            m_metadata_file_writer.flush();

        } catch (FileWriter::OperationFailed& e) {
            SPDLOG_CRITICAL("Failed to write archive file metadata collection in file: {}", metadata_file_path.c_str());
            throw;
        }

        m_global_metadata_db = user_config.global_metadata_db;

        m_global_metadata_db->open();
        m_global_metadata_db->add_archive(m_id_as_string, user_config.storage_id, m_stable_uncompressed_size, m_stable_size, m_creator_id_as_string,
                                          m_creation_num);
        m_global_metadata_db->close();

        // Open log-type dictionary
        string logtype_dict_path = archive_path_string + '/' + cLogTypeDictFilename;
        string logtype_dict_segment_index_path = archive_path_string + '/' + cLogTypeSegmentIndexFilename;
        m_logtype_dict.open(logtype_dict_path, logtype_dict_segment_index_path, cLogtypeDictionaryIdMax);

        // Preallocate logtype dictionary entry
        m_logtype_dict_entry_wrapper = make_unique<LogTypeDictionaryEntry>();

        string jsontype_dict_path = archive_path_string + '/' + cJsonTypeDictFilename;
        string jsontype_dict_segment_index_path = archive_path_string + '/' + cJsonTypeSegmentIndexFilename;
        m_jsontype_dict.open(jsontype_dict_path, jsontype_dict_segment_index_path, cJsontypeDictionaryIdMax);

        m_jsontype_dict_entry_wrapper = make_unique<JsonTypeDictionaryEntry>();

        // Open variable dictionary
        string var_dict_path = archive_path_string + '/' + cVarDictFilename;
        string var_dict_segment_index_path = archive_path_string + '/' + cVarSegmentIndexFilename;
        m_var_dict.open(var_dict_path, var_dict_segment_index_path,
                        EncodedVariableInterpreter::get_var_dict_id_range_end() - EncodedVariableInterpreter::get_var_dict_id_range_begin());

        #if FLUSH_TO_DISK_ENABLED
            // fsync archive directory now that everything in the archive directory has been created
            if (fsync(archive_dir_fd) != 0) {
                SPDLOG_ERROR("Failed to fsync {}, errno={}", archive_path_string.c_str(), errno);
                throw OperationFailed(ErrorCode_errno, __FILENAME__, __LINE__);
            }
        #endif
        if (::close(archive_dir_fd) != 0) {
            // We've already fsynced, so this error shouldn't affect us. Therefore, just log it.
            SPDLOG_WARN("Error when closing file descriptor for {}, errno={}", archive_path_string.c_str(), errno);
        }

        m_path = archive_path_string;
    }

    void Archive::close () {
        // Any mutable files should have been closed and persisted before closing the archive.
        if (!m_mutable_files.empty()) {
            SPDLOG_CRITICAL("Failed to close archive because {} mutable files have not been released.", m_mutable_files.size());
            throw OperationFailed(ErrorCode_Unsupported, __FILENAME__, __LINE__);
        }

        // Close segments if necessary
        if (m_segment_for_files_with_timestamps.is_open()) {
            close_segment_and_persist_file_metadata(m_segment_for_files_with_timestamps, m_files_with_timestamps_in_segment,
                                                    m_logtype_ids_in_segment_for_files_with_timestamps, m_jsontype_ids_in_segment_for_files_with_timestamps,
                                                    m_var_ids_in_segment_for_files_with_timestamps);
            m_logtype_ids_in_segment_for_files_with_timestamps.clear();
            m_jsontype_ids_in_segment_for_files_with_timestamps.clear();
            m_var_ids_in_segment_for_files_with_timestamps.clear();
        }
        if (m_segment_for_files_without_timestamps.is_open()) {
            close_segment_and_persist_file_metadata(m_segment_for_files_without_timestamps, m_files_without_timestamps_in_segment,
                                                    m_logtype_ids_in_segment_for_files_without_timestamps,
                                                    m_jsontype_ids_in_segment_for_files_without_timestamps,
                                                    m_var_ids_in_segment_for_files_without_timestamps);                                        
            m_logtype_ids_in_segment_for_files_without_timestamps.clear();
            m_jsontype_ids_in_segment_for_files_without_timestamps.clear();
            m_var_ids_in_segment_for_files_without_timestamps.clear();
        }

        for (size_t i = 0; i < m_segments_for_columns.size(); i++) {
            if (m_segments_for_columns[i]->is_open()) {
                m_segments_for_columns[i]->close();
            }
        }

        // Persist all metadata including dictionaries
        write_dir_snapshot();

        m_logtype_dict.close();
        m_logtype_dict_entry_wrapper.reset(nullptr);
        m_var_dict.close();
        m_jsontype_dict.close();
        m_jsontype_dict_entry_wrapper.reset(nullptr);

        if (::close(m_segments_dir_fd) != 0) {
            // We've already fsynced, so this error shouldn't affect us. Therefore, just log it.
            SPDLOG_WARN("Error when closing segments directory file descriptor, errno={}", errno);
        }
        m_segments_dir_fd = -1;
        m_segments_dir_path.clear();

        if (::close(m_logs_dir_fd) != 0) {
            // We've already fsynced, so this error shouldn't affect us. Therefore, just log it.
            SPDLOG_WARN("Error when closing logs directory file descriptor, errno={}", errno);
        }
        m_logs_dir_fd = -1;
        m_logs_dir_path.clear();

        m_metadata_file_writer.close();

        m_global_metadata_db = nullptr;

        m_stable_uncompressed_size = 0;
        m_stable_size = 0;

        m_metadata_db.close();

        m_creator_id_as_string.clear();
        m_id_as_string.clear();
        m_path.clear();
    }

    File* Archive::create_in_memory_file (const string& path, const group_id_t group_id, const boost::uuids::uuid& orig_file_id, size_t split_ix) {
        auto file = new InMemoryFile(m_uuid_generator(), orig_file_id, path, m_logs_dir_path, group_id, split_ix);
        m_mutable_files.insert(file);

        return file;
    }

    File* Archive::create_on_disk_file (const string& path, const group_id_t group_id, const boost::uuids::uuid& orig_file_id, size_t split_ix) {
        auto file = new OnDiskFile(m_uuid_generator(), orig_file_id, path, m_logs_dir_path, group_id, split_ix);
        m_mutable_files.insert(file);
        m_on_disk_files.insert(file);

        return file;
    }

    void Archive::open_file (File& file) {
        file.open();
    }

    void Archive::close_file (File& file) {
        file.close();
    }

    bool Archive::is_file_open (File& file) {
        return file.is_open();
    }

    void Archive::change_ts_pattern (File& file, const TimestampPattern* pattern) {
        file.change_ts_pattern(pattern);
    }

    void Archive::write_msg (File& file, epochtime_t timestamp, const string& message, size_t num_uncompressed_bytes) {
        vector<encoded_variable_t> encoded_vars;
        EncodedVariableInterpreter::encode_and_add_to_dictionary(message, *m_logtype_dict_entry_wrapper, m_var_dict, encoded_vars);
        logtype_dictionary_id_t logtype_id;
        if (m_logtype_dict.add_occurrence(m_logtype_dict_entry_wrapper, logtype_id)) {
            m_logtype_dict_entry_wrapper = make_unique<LogTypeDictionaryEntry>();
        }

        file.write_encoded_msg(timestamp, logtype_id, encoded_vars, num_uncompressed_bytes);
    }

    void Archive::write_json_msg (File& file, epochtime_t timestamp, ordered_json& message, size_t num_uncompressed_bytes, std::vector<ordered_json*>& extracted_values) {
        vector<encoded_variable_t> encoded_vars;
        // vector<EncodedJsonVar> encoded_extracted_vars;
        EncodedVariableInterpreter::encode_and_add_to_dictionary(message, *m_jsontype_dict_entry_wrapper, m_logtype_dict, m_var_dict, encoded_vars);
        jsontype_dictionary_id_t jsontype_id;
        if (m_jsontype_dict.add_occurrence(m_jsontype_dict_entry_wrapper, jsontype_id)) {
            m_jsontype_dict_entry_wrapper = make_unique<JsonTypeDictionaryEntry>();
        }

        // EncodedVariableInterpreter::add_extracted_values_to_dictionary(extracted_values, m_var_dict, encoded_extracted_vars);
        // file.write_encoded_json_msg(timestamp, jsontype_id, encoded_vars, num_uncompressed_bytes, encoded_extracted_vars);
        file.write_encoded_json_msg(timestamp, jsontype_id, encoded_vars, num_uncompressed_bytes, extracted_values);
    }

    void Archive::write_dir_snapshot () {
        // Flush and collect OnDiskFiles with dirty metadata
        vector<File*> files_with_dirty_metadata;
        for (auto file : m_on_disk_files) {
            if (file->is_metadata_dirty()) {
                file->flush();
                files_with_dirty_metadata.emplace_back(file);
            }
        }

        // Collect released files with dirty metadata
        for (auto file : m_released_but_dirty_files) {
            files_with_dirty_metadata.emplace_back(file);
        }

        #if FLUSH_TO_DISK_ENABLED
            // fsync logs directory to flush new files' directory entries
            if (0 != fsync(m_logs_dir_fd)) {
                SPDLOG_ERROR("Failed to fsync {}, errno={}", m_logs_dir_path.c_str(), errno);
                throw OperationFailed(ErrorCode_errno, __FILENAME__, __LINE__);
            }
        #endif

        // Flush dictionaries
        m_logtype_dict.write_uncommitted_entries_to_disk();
        m_jsontype_dict.write_uncommitted_entries_to_disk();
        m_var_dict.write_uncommitted_entries_to_disk();

        // Persist files with dirty metadata
        persist_file_metadata(files_with_dirty_metadata);

        // Delete files that have been released
        for (auto file : m_released_but_dirty_files) {
            delete file;
        }
    }

    void Archive::release_and_write_in_memory_file_to_disk (File*& file) {
        // Ensure file has been closed
        if (file->is_open()) {
            SPDLOG_ERROR("An open file cannot be released.");
            throw OperationFailed(ErrorCode_Unsupported, __FILENAME__, __LINE__);
        }

        // Ensure file pointer is a mutable file in this archive
        if (m_mutable_files.count(file) == 0) {
            SPDLOG_ERROR("Given file pointer is not mutable or does not exist in this archive.");
            throw OperationFailed(ErrorCode_Unsupported, __FILENAME__, __LINE__);
        }

        // Remove pointer from mutable files
        m_mutable_files.erase(file);

        // Write file to disk
        auto in_memory_file = reinterpret_cast<InMemoryFile*>(file);
        in_memory_file->write_to_disk();

        // Save file for persistence
        m_released_but_dirty_files.push_back(file);

        // Update archive's stable size
        m_stable_uncompressed_size += file->get_num_uncompressed_bytes();
        m_stable_size += file->get_encoded_size_in_bytes();

        // Remove external access to file
        file = nullptr;
    }

    void Archive::release_on_disk_file (File*& file) {
        // Ensure file has been closed
        if (file->is_open()) {
            SPDLOG_ERROR("An open file cannot be released.");
            throw OperationFailed(ErrorCode_Unsupported, __FILENAME__, __LINE__);
        }

        // Ensure file pointer is an on-disk mutable file in this archive
        if (m_mutable_files.count(file) == 0) {
            SPDLOG_ERROR("Given file pointer is not mutable or does not exist in this archive.");
            throw OperationFailed(ErrorCode_Unsupported, __FILENAME__, __LINE__);
        }
        auto on_disk_file = reinterpret_cast<OnDiskFile*>(file);
        if (m_on_disk_files.count(on_disk_file) == 0) {
            SPDLOG_ERROR("Given file pointer is not an OnDiskFile.");
            throw OperationFailed(ErrorCode_Unsupported, __FILENAME__, __LINE__);
        }

        // Remove pointer from containers of mutable and on-disk files
        m_mutable_files.erase(file);
        m_on_disk_files.erase(on_disk_file);

        m_stable_uncompressed_size += file->get_num_uncompressed_bytes();
        m_stable_size += file->get_encoded_size_in_bytes();

        // Save file for persistence if necessary
        if (file->is_metadata_dirty()) {
            m_released_but_dirty_files.push_back(file);
        } else {
            delete file;
        }

        // Remove external access to file
        file = nullptr;
    }

    void Archive::append_file_to_segment (File*& file, Segment& segment, vector<Segment*>& column_segments, unordered_set<logtype_dictionary_id_t>& logtype_ids_in_segment,
                                          unordered_set<jsontype_dictionary_id_t>& jsontype_ids_in_segment,
                                          unordered_set<variable_dictionary_id_t>& var_ids_in_segment, vector<File*>& files_in_segment)
    {
        if (!segment.is_open()) {
            segment.open(m_segments_dir_path, m_next_segment_id, m_compression_level);
        }

        for (size_t i = 0; i < column_segments.size(); i++) {
            if (!column_segments[i]->is_open()) {
                column_segments[i]->open(m_column_segments_dir_path, m_preparsed_keys_original[i], m_next_segment_id, m_compression_level);
            }
        }

        m_next_segment_id++;

        file->append_to_segment(m_logtype_dict, m_jsontype_dict, segment, column_segments, logtype_ids_in_segment, jsontype_ids_in_segment, var_ids_in_segment);
        files_in_segment.emplace_back(file);

        // TODO(Rui): close column segment
        // Close current segment if its uncompressed size is greater than the target
        if (segment.get_uncompressed_size() >= m_target_segment_uncompressed_size) {
            close_segment_and_persist_file_metadata(segment, files_in_segment, logtype_ids_in_segment, jsontype_ids_in_segment, var_ids_in_segment);
            
            for (size_t i = 0; i < column_segments.size(); i++) {
                if (!column_segments[i]->is_open()) {
                    column_segments[i]->close();
                }
            }

            jsontype_ids_in_segment.clear();
            logtype_ids_in_segment.clear();
            var_ids_in_segment.clear();
        }
    }

    /**
     * There are two data paths after the file is marked ready for a segment
     * 1) Given file has a timestamp, so append to segment only if enough files are buffered
     * 2) Given file lacks a timestamp, append to segment immediately
     */
    void Archive::mark_file_ready_for_segment (File*& file) {
        // Check if file actually exists in our tracked file container
        if (m_mutable_files.count(file) == 0) {
            SPDLOG_CRITICAL("Unable to mark a file ready for segment for file that is not tracked by the current archive");
            throw OperationFailed(ErrorCode_Unsupported, __FILENAME__, __LINE__);
        }

        // Remove file pointer visibility from outside of the archive
        m_mutable_files.erase(file);

        if (file->has_ts_pattern()) {
            append_file_to_segment(file, m_segment_for_files_with_timestamps, m_segments_for_columns, m_logtype_ids_in_segment_for_files_with_timestamps,
                                   m_jsontype_ids_in_segment_for_files_with_timestamps, m_var_ids_in_segment_for_files_with_timestamps,
                                   m_files_with_timestamps_in_segment);
        } else {
            append_file_to_segment(file, m_segment_for_files_without_timestamps, m_segments_for_columns, m_logtype_ids_in_segment_for_files_without_timestamps,
                                   m_jsontype_ids_in_segment_for_files_without_timestamps, m_var_ids_in_segment_for_files_without_timestamps,
                                   m_files_without_timestamps_in_segment);
        }

        // Make sure file pointer is nulled and cannot be accessed outside
        file = nullptr;
    }

    void Archive::persist_file_metadata (const vector<File*>& files) {
        if (files.empty()) {
            return;
        }

        m_metadata_db.update_files(files);

        m_global_metadata_db->update_metadata_for_files(m_id_as_string, files);

        // Mark files' metadata as clean
        for (auto file : files) {
            file->mark_metadata_as_clean();
        }
    }

    void Archive::close_segment_and_persist_file_metadata (Segment& segment, std::vector<File*>& files,
                                                           const unordered_set<logtype_dictionary_id_t>& segment_logtype_ids,
                                                           const unordered_set<jsontype_dictionary_id_t>& segment_jsontype_ids,
                                                           const unordered_set<variable_dictionary_id_t>& segment_var_ids)
    {
        auto segment_id = segment.get_id();
        m_logtype_dict.index_segment(segment_id, segment_logtype_ids);
        m_jsontype_dict.index_segment(segment_id, segment_jsontype_ids);
        m_var_dict.index_segment(segment_id, segment_var_ids);

        m_stable_size += segment.get_compressed_size();

        segment.close();

        #if FLUSH_TO_DISK_ENABLED
            // fsync segments directory to flush segment's directory entry
            if (fsync(m_segments_dir_fd) != 0) {
                SPDLOG_ERROR("Failed to fsync {}, errno={}", m_segments_dir_path.c_str(), errno);
                throw OperationFailed(ErrorCode_errno, __FILENAME__, __LINE__);
            }
        #endif

        // Flush dictionaries
        // auto logtype_entries = m_logtype_dict.get_uncommitted_entries();
        // std::ofstream myfile;

        // if (logtype_entries.size() != 0) {
        //     myfile.open(m_segments_dir_path + "logtype.entries");
        //     for (auto entry : logtype_entries)
        //         myfile << entry->get_value() << std::endl;
        //     myfile.close();
        //     std::cout << "Logtype entry number: ";
        //     std::cout << logtype_entries.size() << std::endl;
        // }
        m_logtype_dict.write_uncommitted_entries_to_disk();

        // auto jsontype_entries = m_jsontype_dict.get_uncommitted_entries();
        // if (jsontype_entries.size() != 0) {
        //     myfile.open(m_segments_dir_path + "jsontype.entries");
        //     for (auto entry : jsontype_entries)
        //         myfile << entry->get_value() << std::endl;
        //     myfile.close();
        //     std::cout << "Jsontype entry number: ";
        //     std::cout << jsontype_entries.size() << std::endl;
        // }
        m_jsontype_dict.write_uncommitted_entries_to_disk();

        // auto variable_entries = m_var_dict.get_uncommitted_entries();
        // if (variable_entries.size() != 0) {
        //     myfile.open(m_segments_dir_path + "variable.entries");
        //     for (auto entry : variable_entries)
        //         myfile << entry->get_value() << std::endl;
        //     myfile.close();
        //     std::cout << "Variable entry number: ";
        //     std::cout << variable_entries.size() << std::endl;
        // }
        m_var_dict.write_uncommitted_entries_to_disk();

        for (auto file : files) {
            file->mark_as_in_committed_segment();
        }

        m_global_metadata_db->open();
        persist_file_metadata(files);
        update_metadata();
        m_global_metadata_db->close();

        for (auto file : files) {
            file->cleanup_after_segment_insertion();
            m_stable_uncompressed_size += file->get_num_uncompressed_bytes();
            delete file;
        }
        files.clear();
    }

    void Archive::add_empty_directories (const vector<string>& empty_directory_paths) {
        if (empty_directory_paths.empty()) {
            return;
        }

        m_metadata_db.add_empty_directories(empty_directory_paths);
    }

    size_t Archive::get_stable_uncompressed_size () const {
        size_t uncompressed_size = m_stable_uncompressed_size;

        // Add size of on-disk files
        for (auto file : m_on_disk_files) {
            uncompressed_size += file->get_num_uncompressed_bytes();
        }

        // Add size of files in an unclosed segment
        for (auto file : m_files_with_timestamps_in_segment) {
            uncompressed_size += file->get_num_uncompressed_bytes();
        }
        for (auto file : m_files_without_timestamps_in_segment) {
            uncompressed_size += file->get_num_uncompressed_bytes();
        }

        return uncompressed_size;
    }

    size_t Archive::get_stable_size () const {
        size_t on_disk_size = m_stable_size + m_logtype_dict.get_on_disk_size() + m_var_dict.get_on_disk_size() + m_jsontype_dict.get_on_disk_size();

        // Add size of on-disk files
        for (auto file : m_on_disk_files) {
            on_disk_size += file->get_encoded_size_in_bytes();
        }

        // Add size of unclosed segment
        if (m_segment_for_files_without_timestamps.is_open()) {
            on_disk_size += m_segment_for_files_without_timestamps.get_compressed_size();
        }

        return on_disk_size;
    }

    void Archive::update_metadata () {
        auto stable_uncompressed_size = get_stable_uncompressed_size();
        auto stable_size = get_stable_size();

        m_metadata_file_writer.seek_from_current(-((int64_t)(sizeof(m_stable_uncompressed_size) + sizeof(m_stable_size))));
        m_metadata_file_writer.write_numeric_value(stable_uncompressed_size);
        m_metadata_file_writer.write_numeric_value(stable_size);

        m_global_metadata_db->update_archive_size(m_id_as_string, stable_uncompressed_size, stable_size);

        if (m_print_archive_stats_progress) {
            nlohmann::json json_msg;
            json_msg["id"] = m_id_as_string;
            json_msg["uncompressed_size"] = stable_uncompressed_size;
            json_msg["size"] = stable_size;
            std::cout << json_msg.dump(-1, ' ', true, nlohmann::json::error_handler_t::ignore) << std::endl;
        }
    }
} }

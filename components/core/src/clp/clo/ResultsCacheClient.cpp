#include "ResultsCacheClient.hpp"

namespace clp::clo {
ResultsCacheClient::ResultsCacheClient(
        std::string const& uri,
        std::string const& collection,
        uint64_t batch_size,
        uint64_t target_num_latest_results
)
        : m_batch_size(batch_size),
          m_target_num_latest_results(target_num_latest_results) {
    try {
        auto mongo_uri = mongocxx::uri(uri);
        m_client = mongocxx::client(mongo_uri);
        m_collection = m_client[mongo_uri.database()][collection];
    } catch (mongocxx::exception const& e) {
        throw OperationFailed(ErrorCode::ErrorCode_BadParam_DB_URI, __FILE__, __LINE__);
    }
}

void ResultsCacheClient::flush() {
    while (false == m_top_results.empty()) {
        auto const& result = m_top_results.top();
        m_results.emplace_back(bsoncxx::builder::basic::make_document(
                bsoncxx::builder::basic::kvp("original_path", std::move(result->original_path)),
                bsoncxx::builder::basic::kvp("message", std::move(result->message)),
                bsoncxx::builder::basic::kvp("timestamp", result->timestamp)
        ));
        m_top_results.pop();
    }

    try {
        if (false == m_results.empty()) {
            m_collection.insert_many(m_results);
            m_results.clear();
        }
    } catch (mongocxx::exception const& e) {
        throw OperationFailed(ErrorCode::ErrorCode_Failure_DB_Bulk_Write, __FILE__, __LINE__);
    }
}

void ResultsCacheClient::add_result(
        std::string const& original_path,
        std::string const& message,
        epochtime_t timestamp
) {
    if (m_target_num_latest_results == 0) {
        try {
            auto document = bsoncxx::builder::basic::make_document(
                    bsoncxx::builder::basic::kvp("original_path", original_path),
                    bsoncxx::builder::basic::kvp("message", message),
                    bsoncxx::builder::basic::kvp("timestamp", timestamp)
            );

            m_results.push_back(std::move(document));

            if (m_results.size() >= m_batch_size) {
                m_collection.insert_many(m_results);
                m_results.clear();
            }
        } catch (mongocxx::exception const& e) {
            throw OperationFailed(ErrorCode::ErrorCode_Failure_DB_Bulk_Write, __FILE__, __LINE__);
        }
    } else if (m_top_results.size() < m_target_num_latest_results) {
        m_top_results.emplace(std::make_unique<QueryResult>(original_path, message, timestamp));
    } else if (m_top_results.top()->timestamp < timestamp) {
        m_top_results.pop();
        m_top_results.push(std::make_unique<QueryResult>(original_path, message, timestamp));
    }
}
}  // namespace clp::clo

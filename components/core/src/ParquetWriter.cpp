#include "ParquetWriter.hpp"

#include <boost/uuid/uuid_io.hpp>
#include <parquet/exception.h>
#include <parquet/file_writer.h>
#include <parquet/api/reader.h>
#include <parquet/api/writer.h>
#include <parquet/stream_writer.h>

void ParquetWriter::init (boost::uuids::uuid id, boost::uuids::uuid creator_id, int compression_level, const boost::filesystem::path& output_dir) {
    auto output_path = output_dir / boost::uuids::to_string(id);
    PARQUET_ASSIGN_OR_THROW(m_outfile, arrow::io::FileOutputStream::Open(output_path.string()));

    m_builder.compression(parquet::Compression::ZSTD);
    m_builder.compression_level(compression_level);

    m_creator_id_as_string = boost::uuids::to_string(creator_id);
    parquet::StreamWriter os{parquet::ParquetFileWriter::Open(m_outfile, get_schema(), m_builder.build())};

}

void ParquetWriter::add_metadata (const std::string& key, const std::string& value) {
    m_keys.push_back(key);
    m_values.push_back(value);
}

std::shared_ptr<parquet::schema::GroupNode> ParquetWriter::get_schema () {
    parquet::schema::NodeVector fields;


}
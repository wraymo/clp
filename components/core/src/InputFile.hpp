#ifndef INPUTFILE_HPP
#define INPUTFILE_HPP

#include <string>
#include <utility>

class InputFile {
public:
    enum class FileType { TEXT, JSON };

    InputFile (std::string path, const uint64_t start_rowgroup, const uint64_t start_row): m_type(FileType::TEXT), m_path(std::move(path)),
                m_start_rowgroup(start_rowgroup), m_start_row(start_row), m_end_rowgroup(start_rowgroup), m_end_row(start_row) {}

    void close (uint64_t end_rowgroup, uint64_t end_row);

    FileType get_type () { return m_type; }
    void set_type (FileType type) { m_type = type; }

    uint64_t get_start_rowgroup () const { return m_start_rowgroup; }
    uint64_t get_start_row () const { return m_start_row; }
    uint64_t get_end_rowgroup () const { return m_end_rowgroup; }
    uint64_t get_end_row () const { return m_end_row; }

    std::string get_path () { return m_path; }

private:
    FileType m_type;
    std::string m_path;

    uint64_t m_start_rowgroup;
    uint64_t m_start_row;
    uint64_t m_end_rowgroup;
    uint64_t m_end_row;
};
#endif //INPUTFILE_HPP

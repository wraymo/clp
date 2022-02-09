#include "InputFile.hpp"

void InputFile::close(const uint64_t end_rowgroup, const uint64_t end_row) {
    m_end_rowgroup = end_rowgroup;
    m_end_row = end_row;
}

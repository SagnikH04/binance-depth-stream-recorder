#include "csv_writer.hpp"
#include <stdexcept>

CsvWriter::CsvWriter(const std::string& path) : path_(path) {
    file_.open(path, std::ios::out | std::ios::trunc);
    if (!file_.is_open()) {
        throw std::runtime_error("Failed to open CSV file for writing: " + path);
    }
}

CsvWriter::~CsvWriter() {
    if (file_.is_open()) {
        file_.flush();
        file_.close();
    }
}

void CsvWriter::write_header(const std::vector<std::string>& fields) {
    write_row(fields);
}

void CsvWriter::write_row(const std::vector<std::string>& fields) {
    for (size_t i = 0; i < fields.size(); i++) {
        if (i > 0) file_ << ',';
        file_ << escape_field(fields[i]);
    }
    file_ << '\n';
    if (file_.fail()) {
        throw std::runtime_error("CSV write failed: " + path_);
    }
}

void CsvWriter::flush() {
    file_.flush();
}

std::string CsvWriter::escape_field(std::string_view field) {
    bool needs_quoting = false;
    for (char c : field) {
        if (c == ',' || c == '"' || c == '\r' || c == '\n') {
            needs_quoting = true;
            break;
        }
    }
    if (!needs_quoting) {
        return std::string(field);
    }

    std::string result;
    result.reserve(field.size() + 4);
    result += '"';
    for (char c : field) {
        if (c == '"') {
            result += "\"\"";
        } else {
            result += c;
        }
    }
    result += '"';
    return result;
}

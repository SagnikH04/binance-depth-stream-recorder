#include "csv_reader.hpp"
#include <stdexcept>

CsvReader::CsvReader(const std::string& path) : path_(path) {
    file_.open(path, std::ios::in);
    if (!file_.is_open()) {
        throw std::runtime_error("Failed to open CSV file for reading: " + path);
    }
}

CsvReader::~CsvReader() {
    if (file_.is_open()) {
        file_.close();
    }
}

int CsvReader::peek_char() {
    return file_.peek();
}

int CsvReader::read_char() {
    return file_.get();
}

bool CsvReader::read_row(std::vector<std::string>& out) {
    out.clear();

    if (file_.eof() || peek_char() == EOF) {
        return false;
    }

    row_number_++;
    std::string field;
    bool in_quotes = false;
    bool field_started = false;

    while (true) {
        int ch = read_char();

        if (ch == EOF) {
            if (in_quotes) {
                throw std::runtime_error(
                    "Unterminated quoted field at row " + std::to_string(row_number_) +
                    " in " + path_);
            }
            out.push_back(field);
            return !out.empty() || field_started;
        }

        if (in_quotes) {
            if (ch == '"') {
                int next = peek_char();
                if (next == '"') {
                    read_char();
                    field += '"';
                } else {
                    in_quotes = false;
                }
            } else {
                field += static_cast<char>(ch);
            }
        } else {
            if (ch == '"' && field.empty() && !field_started) {
                in_quotes = true;
                field_started = true;
            } else if (ch == ',') {
                out.push_back(field);
                field.clear();
                field_started = false;
            } else if (ch == '\r') {
                int next = peek_char();
                if (next == '\n') {
                    read_char();
                }
                out.push_back(field);
                return true;
            } else if (ch == '\n') {
                out.push_back(field);
                return true;
            } else {
                field += static_cast<char>(ch);
                field_started = true;
            }
        }
    }
}

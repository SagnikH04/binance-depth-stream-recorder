#pragma once
#include <fstream>
#include <string>
#include <vector>

class CsvReader {
public:
    explicit CsvReader(const std::string& path);
    ~CsvReader();

    bool read_row(std::vector<std::string>& out);
    size_t row_number() const { return row_number_; }

private:
    std::ifstream file_;
    std::string path_;
    size_t row_number_ = 0;

    int peek_char();
    int read_char();
};

#pragma once
#include <fstream>
#include <string>
#include <string_view>
#include <vector>

class CsvWriter {
public:
    explicit CsvWriter(const std::string& path);
    ~CsvWriter();

    void write_header(const std::vector<std::string>& fields);
    void write_row(const std::vector<std::string>& fields);
    void flush();

    static std::string escape_field(std::string_view field);

private:
    std::ofstream file_;
    std::string path_;
};

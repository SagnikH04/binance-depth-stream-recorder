#pragma once
#include "market_event.hpp"
#include <optional>
#include <string>
#include <vector>

struct AppConfig {
    Venue venue = Venue::Spot;
    std::vector<std::string> symbols;
    std::string output_dir = "./output";
    std::optional<int> duration_seconds;
    bool replay_mode = false;
    std::string replay_file;
};

AppConfig parse_args(int argc, char** argv);
void print_usage();

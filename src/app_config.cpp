#include "app_config.hpp"
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <sys/stat.h>

void print_usage() {
    std::cout <<
        "Usage: binance_capture [OPTIONS]\n"
        "\n"
        "Live capture mode:\n"
        "  --venue spot|usdm       Exchange venue (required for live)\n"
        "  --symbols SYM1,SYM2     Comma-separated symbol list (required for live)\n"
        "  --output-dir DIR        Output directory (required)\n"
        "  --duration SECONDS      Capture duration in seconds (optional)\n"
        "\n"
        "Replay mode:\n"
        "  --replay FILE           Replay a market_data CSV (no network calls)\n"
        "  --output-dir DIR        Output directory for replay orderbook\n"
        "\n"
        "Other:\n"
        "  --help                  Show this help message\n"
        "\n"
        "Examples:\n"
        "  binance_capture --venue spot --symbols BTCUSDT --output-dir ./output --duration 120\n"
        "  binance_capture --venue spot --symbols BTCUSDT,ETHUSDT --output-dir ./output --duration 120\n"
        "  binance_capture --venue usdm --symbols BTCUSDT --output-dir ./output --duration 120\n"
        "  binance_capture --replay ./output/market_data_spot_BTCUSDT_2025-01-15.csv --output-dir ./output\n";
}

static std::vector<std::string> split_symbols(const std::string& s) {
    std::vector<std::string> symbols;
    std::istringstream iss(s);
    std::string symbol;
    while (std::getline(iss, symbol, ',')) {
        std::transform(symbol.begin(), symbol.end(), symbol.begin(),
                       [](unsigned char c) { return std::toupper(c); });
        if (!symbol.empty()) {
            symbols.push_back(symbol);
        }
    }
    return symbols;
}

AppConfig parse_args(int argc, char** argv) {
    AppConfig config;
    bool has_venue = false;
    bool has_symbols = false;
    bool has_output = false;

    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];

        if (arg == "--help") {
            print_usage();
            std::exit(0);
        } else if (arg == "--venue" && i + 1 < argc) {
            std::string v = argv[++i];
            config.venue = parse_venue(v);
            has_venue = true;
        } else if (arg == "--symbols" && i + 1 < argc) {
            config.symbols = split_symbols(argv[++i]);
            has_symbols = true;
        } else if (arg == "--output-dir" && i + 1 < argc) {
            config.output_dir = argv[++i];
            has_output = true;
        } else if (arg == "--duration" && i + 1 < argc) {
            config.duration_seconds = std::stoi(argv[++i]);
            if (*config.duration_seconds <= 0) {
                throw std::invalid_argument("Duration must be positive");
            }
        } else if (arg == "--replay" && i + 1 < argc) {
            config.replay_mode = true;
            config.replay_file = argv[++i];
        } else {
            throw std::invalid_argument("Unknown argument: " + arg);
        }
    }

    if (config.replay_mode) {
        if (!has_output) {
            throw std::invalid_argument("Replay mode requires --output-dir");
        }
    } else {
        if (!has_venue) {
            throw std::invalid_argument("Live mode requires --venue");
        }
        if (!has_symbols || config.symbols.empty()) {
            throw std::invalid_argument("Live mode requires --symbols");
        }
        if (!has_output) {
            throw std::invalid_argument("Live mode requires --output-dir");
        }
    }

    // Ensure the requested output directory is available before writers open files.
    mkdir(config.output_dir.c_str(), 0755);

    return config;
}

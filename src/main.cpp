#include "app_config.hpp"
#include "binance_ws_client.hpp"
#include "event_processor.hpp"
#include "logging.hpp"
#include "replay.hpp"
#include "time_utils.hpp"

#include <atomic>
#include <csignal>
#include <iostream>

static std::atomic<bool> g_stop_flag{false};

static void signal_handler(int) {
    g_stop_flag.store(true);
}

int main(int argc, char** argv) {
    try {
        if (argc < 2) {
            print_usage();
            return 1;
        }

        auto app_config = parse_args(argc, argv);

        if (app_config.replay_mode) {
            return run_replay(app_config.replay_file, app_config.output_dir);
        }

        // Live mode
        std::signal(SIGINT, signal_handler);
        std::signal(SIGTERM, signal_handler);

        std::string capture_date = utc_date_yyyy_mm_dd();
        EventProcessor event_processor(app_config.venue, app_config.symbols,
                                       app_config.output_dir, capture_date);

        BinanceWsClient client(app_config, event_processor, g_stop_flag);
        client.run();

        event_processor.print_counters();
        return 0;

    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << "\n";
        return 1;
    }
}

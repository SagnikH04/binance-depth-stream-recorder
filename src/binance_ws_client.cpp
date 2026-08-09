#include "binance_ws_client.hpp"
#include "logging.hpp"
#include "time_utils.hpp"

#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/beast/websocket/ssl.hpp>
#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl/stream.hpp>

#include <openssl/ssl.h>
#include <algorithm>
#include <chrono>
#include <cctype>
#include <thread>

namespace beast = boost::beast;
namespace websocket = beast::websocket;
namespace net = boost::asio;
namespace ssl = net::ssl;
using tcp = net::ip::tcp;

BinanceWsClient::BinanceWsClient(const AppConfig& config, EventProcessor& processor,
                                 std::atomic<bool>& stop_flag)
    : config_(config), processor_(processor), stop_flag_(stop_flag) {}

std::string BinanceWsClient::build_host() const {
    if (config_.venue == Venue::Spot) {
        return "stream.binance.com";
    }
    return "fstream.binance.com";
}

std::string BinanceWsClient::build_port() const {
    if (config_.venue == Venue::Spot) {
        return "9443";
    }
    return "443";
}

std::string BinanceWsClient::build_target() const {
    std::string prefix;
    if (config_.venue == Venue::Spot) {
        prefix = "/stream?streams=";
    } else {
        prefix = "/public/stream?streams=";
    }

    std::string streams;
    for (size_t i = 0; i < config_.symbols.size(); i++) {
        std::string sym_lower = config_.symbols[i];
        std::transform(sym_lower.begin(), sym_lower.end(), sym_lower.begin(),
                       [](unsigned char c) { return std::tolower(c); });

        if (!streams.empty()) streams += '/';
        streams += sym_lower + "@depth@100ms";
        streams += '/';
        streams += sym_lower + "@depth5@100ms";
        streams += '/';
        streams += sym_lower + "@trade";
    }

    return prefix + streams;
}

int BinanceWsClient::backoff_seconds(int attempt) const {
    static const int backoffs[] = {1, 2, 5, 10, 30};
    int idx = std::min(attempt, 4);
    return backoffs[idx];
}

void BinanceWsClient::connect_and_read(std::chrono::steady_clock::time_point program_start) {
    auto host = build_host();
    auto port = build_port();
    auto target = build_target();

    log_info("Connecting to " + host + ":" + port + target);

    net::io_context ioc;
    ssl::context ctx{ssl::context::tlsv12_client};

    ctx.set_default_verify_paths();
    ctx.set_verify_mode(ssl::verify_peer);

    tcp::resolver resolver{ioc};

    using ssl_stream_type = ssl::stream<tcp::socket>;
    websocket::stream<ssl_stream_type> wss{ioc, ctx};

    auto const results = resolver.resolve(host, port);
    auto ep = net::connect(beast::get_lowest_layer(wss), results);

    // Set SNI
    if (!SSL_set_tlsext_host_name(wss.next_layer().native_handle(), host.c_str())) {
        throw beast::system_error(
            beast::error_code(static_cast<int>(::ERR_get_error()),
                              net::error::get_ssl_category()),
            "Failed to set SNI hostname");
    }

    // Explicit hostname verification (OpenSSL 1.1+)
    SSL_set1_host(wss.next_layer().native_handle(), host.c_str());

    wss.next_layer().handshake(ssl::stream_base::client);

    std::string ws_host = host + ":" + std::to_string(ep.port());
    wss.handshake(ws_host, target);

    log_info("WebSocket connected (epoch=" + std::to_string(conn_epoch_) + ")");

    beast::flat_buffer buffer;
    while (!stop_flag_.load()) {
        if (config_.duration_seconds.has_value()) {
            auto elapsed = std::chrono::steady_clock::now() - program_start;
            auto secs = std::chrono::duration_cast<std::chrono::seconds>(elapsed).count();
            if (secs >= *config_.duration_seconds) {
                log_info("Duration limit reached (" +
                         std::to_string(*config_.duration_seconds) + "s)");
                break;
            }
        }

        wss.read(buffer);
        auto recv_ts = now_wall_clock();

        std::string msg = beast::buffers_to_string(buffer.data());
        buffer.consume(buffer.size());

        processor_.process_raw_ws_message(msg, recv_ts, 0, conn_epoch_, conn_seq_);
        conn_seq_++;
    }

    beast::error_code ec;
    wss.close(websocket::close_code::normal, ec);
}

void BinanceWsClient::run() {
    int attempt = 0;
    auto program_start = std::chrono::steady_clock::now();

    while (!stop_flag_.load()) {
        if (config_.duration_seconds.has_value()) {
            auto elapsed = std::chrono::steady_clock::now() - program_start;
            auto secs = std::chrono::duration_cast<std::chrono::seconds>(elapsed).count();
            if (secs >= *config_.duration_seconds) {
                break;
            }
        }

        try {
            connect_and_read(program_start);
            break;
        } catch (const std::exception& e) {
            if (stop_flag_.load()) break;

            log_error(std::string("Connection error: ") + e.what());

            processor_.record_reconnect();
            conn_epoch_++;
            conn_seq_ = 0;
            processor_.mark_all_books_stale_on_reconnect();

            int wait = backoff_seconds(attempt);
            log_info("Reconnecting in " + std::to_string(wait) + "s (epoch=" +
                     std::to_string(conn_epoch_) + ")");
            std::this_thread::sleep_for(std::chrono::seconds(wait));
            attempt++;
        }
    }

    processor_.flush();
}

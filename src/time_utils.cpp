#include "time_utils.hpp"
#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>

ReceiveTimestamp now_wall_clock() {
    auto now = std::chrono::system_clock::now();
    auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
        now.time_since_epoch()).count();
    ReceiveTimestamp ts;
    ts.tsec = ns / 1000000000LL;
    ts.tnsec = static_cast<int32_t>(ns % 1000000000LL);
    return ts;
}

std::string utc_date_yyyy_mm_dd() {
    auto now = std::chrono::system_clock::now();
    auto tt = std::chrono::system_clock::to_time_t(now);
    std::tm utc_tm{};
    gmtime_r(&tt, &utc_tm);
    std::ostringstream oss;
    oss << std::put_time(&utc_tm, "%Y-%m-%d");
    return oss.str();
}

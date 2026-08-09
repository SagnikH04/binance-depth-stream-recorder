#pragma once
#include <cstdint>
#include <string>

struct ReceiveTimestamp {
    int64_t tsec;
    int32_t tnsec;
};

ReceiveTimestamp now_wall_clock();
std::string utc_date_yyyy_mm_dd();

#pragma once
#include <cstdint>
#include <string_view>

constexpr int64_t PRICE_SCALE = 100000000LL;
constexpr int64_t QTY_SCALE   = 100000000LL;

int64_t parse_scaled_decimal(std::string_view value, int64_t scale);

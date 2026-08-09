#include "decimal_scaler.hpp"
#include <stdexcept>
#include <string>

int64_t parse_scaled_decimal(std::string_view value, int64_t scale) {
    if (value.empty()) {
        throw std::invalid_argument("empty decimal string");
    }

    size_t pos = 0;
    int64_t sign = 1;

    if (value[pos] == '-') {
        sign = -1;
        pos++;
    } else if (value[pos] == '+') {
        pos++;
    }

    if (pos >= value.size()) {
        throw std::invalid_argument("no digits in decimal string");
    }

    // Compute scale_digits: number of fractional digits that scale represents
    int scale_digits = 0;
    {
        int64_t s = scale;
        while (s > 1) {
            if (s % 10 != 0) {
                throw std::invalid_argument("scale must be a power of 10");
            }
            s /= 10;
            scale_digits++;
        }
    }

    // Parse integer part
    int64_t integer_part = 0;
    bool has_integer = false;
    bool has_dot = false;

    while (pos < value.size() && value[pos] != '.') {
        char c = value[pos];
        if (c < '0' || c > '9') {
            throw std::invalid_argument(
                std::string("invalid character in decimal: '") + c + "'");
        }
        has_integer = true;
        int64_t prev = integer_part;
        integer_part = integer_part * 10 + (c - '0');
        if (integer_part / 10 != prev) {
            throw std::overflow_error("integer part overflow");
        }
        pos++;
    }

    // Parse fractional part
    int frac_digits = 0;
    int64_t frac_part = 0;

    if (pos < value.size() && value[pos] == '.') {
        has_dot = true;
        pos++;
        while (pos < value.size()) {
            char c = value[pos];
            if (c < '0' || c > '9') {
                throw std::invalid_argument(
                    std::string("invalid character in fraction: '") + c + "'");
            }
            if (frac_digits < scale_digits) {
                frac_part = frac_part * 10 + (c - '0');
            }
            // Digits beyond scale precision are truncated
            frac_digits++;
            pos++;
        }
    }

    if (!has_integer && !has_dot) {
        throw std::invalid_argument("no digits in decimal string");
    }
    if (!has_integer && has_dot && frac_digits == 0) {
        throw std::invalid_argument("lone dot is not a valid decimal");
    }

    // Pad fractional part to scale_digits
    for (int i = frac_digits; i < scale_digits; i++) {
        frac_part *= 10;
    }

    // Combine: integer_part * scale + frac_part
    int64_t result = integer_part * scale;
    if (scale != 0 && result / scale != integer_part) {
        throw std::overflow_error("scaled value overflow");
    }
    result += frac_part;
    result *= sign;

    return result;
}

# CLAUDE.md

## Project

Binance depth-stream recorder with local order-book CSV output. C++17, Boost.Beast, nlohmann::json.

## Build

```bash
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_FLAGS="-Wall -Wextra -O2"
cmake --build build --parallel
```

## Test

```bash
ctest --test-dir build --output-on-failure
```

## Run

```bash
./build/binance_capture --venue spot --symbols BTCUSDT --output-dir ./output --duration 120
./build/binance_capture --replay tests/fixtures/sample_market_data.csv --output-dir ./output
```

## Key constraints

- No double/float for price/quantity scaling (PRICE_SCALE = QTY_SCALE = 100000000)
- Orderbook CSV must have exactly 26 columns per data row
- payload_json is inner data object only, not combined stream envelope
- Trades do not mutate the order book
- Replay makes no network calls
- depth5 is a top-5 refresh, not a full REST snapshot

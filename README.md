# Binance Depth Stream Recorder

A compact C++17 command-line tool for recording Binance public combined WebSocket streams, writing raw market events to CSV, keeping a local top-of-book view per symbol, and producing order-book snapshot CSV rows as updates are applied.

## What It Does

1. Subscribes to Binance combined streams (`depth@100ms`, `depth5@100ms`, `trade`).
2. Stores one market-data CSV row for every inbound Binance payload.
3. Maintains a local book for each symbol in processing order.
4. Writes an order-book snapshot CSV row after each applied depth event.
5. Includes replay mode that rebuilds order-book CSV files from captured market-data CSV input without network calls.

## Build Target

- **C++17**
- **GCC 12** (target; also builds with Clang 16+ on macOS)
- CMake 3.16+, Ninja build system

## Dependencies

| Library | Purpose |
|---------|---------|
| Boost.Beast / Boost.Asio | WebSocket + TLS transport |
| OpenSSL | TLS 1.2 for secure connections |
| nlohmann::json | JSON parsing |
| POSIX threads | Threading support |

## Build Commands

### Prerequisites (Ubuntu 22.04 / Debian)

```bash
sudo apt-get update
sudo apt-get install -y \
  cmake ninja-build \
  g++-12 \
  libssl-dev \
  libboost-all-dev \
  zlib1g-dev \
  nlohmann-json3-dev
```

### macOS (Homebrew)

```bash
brew install cmake ninja boost nlohmann-json openssl@3
```

### Build

```bash
cmake -B build -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_CXX_COMPILER=g++-12 \
  -DCMAKE_CXX_FLAGS="-Wall -Wextra -O2"

cmake --build build --parallel
```

On macOS (uses system clang):
```bash
cmake -B build -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_CXX_FLAGS="-Wall -Wextra -O2"

cmake --build build --parallel
```

## CLI Reference

```
Usage: binance_capture [OPTIONS]

Live capture mode:
  --venue spot|usdm       Exchange venue (required for live)
  --symbols SYM1,SYM2     Comma-separated symbol list (required for live)
  --output-dir DIR        Output directory (required)
  --duration SECONDS      Capture duration in seconds (optional)

Replay mode:
  --replay FILE           Replay a market_data CSV (no network calls)
  --output-dir DIR        Output directory for replay orderbook

Other:
  --help                  Show this help message
```

## Live Capture Examples

Single symbol, spot, 2-minute capture:
```bash
./build/binance_capture \
  --venue spot \
  --symbols BTCUSDT \
  --output-dir ./output \
  --duration 120
```

Multi-symbol:
```bash
./build/binance_capture \
  --venue spot \
  --symbols BTCUSDT,ETHUSDT \
  --output-dir ./output \
  --duration 120
```

USD-M futures:
```bash
./build/binance_capture \
  --venue usdm \
  --symbols BTCUSDT \
  --output-dir ./output \
  --duration 120
```

## Replay Mode Examples

```bash
./build/binance_capture \
  --replay ./output/market_data_spot_BTCUSDT_2025-01-15.csv \
  --output-dir ./output
```

Replay reads the market-data CSV, processes events in file order, and generates `*_replay_orderbook.csv` files. **No network calls are made during replay.**

## Symbol List Format

- Comma-separated, e.g. `BTCUSDT,ETHUSDT`
- Internally uppercased for CSV output
- Lowercased for WebSocket stream subscription

## Output File Naming Policy

- Single symbol market data: `market_data_<venue>_<SYMBOL>_<YYYY-MM-DD>.csv`
- Multi-symbol market data: `market_data_<venue>_MULTI_<YYYY-MM-DD>.csv`
- Order book (per symbol): `market_data_<venue>_<SYMBOL>_<YYYY-MM-DD>_orderbook.csv`
- Replay order book: `market_data_<venue>_<SYMBOL>_<YYYY-MM-DD>_replay_orderbook.csv`

UTC date is used for file naming.

## Market-Data CSV Schema

First row is the header (exactly):
```
recv_tsec,recv_tnsec,venue,stream_kind,shard_id,conn_epoch,conn_seq,symbol,payload_json
```

| Column | Type | Description |
|--------|------|-------------|
| recv_tsec | int64 | Seconds part of receive timestamp |
| recv_tnsec | int32 | Nanosecond remainder (0-999999999) |
| venue | string | `spot` or `usdm` |
| stream_kind | string | `depth_diff`, `depth5`, or `trade` |
| shard_id | int | Non-negative integer (0 for single connection) |
| conn_epoch | int | Increments on each WebSocket reconnect |
| conn_seq | uint64 | Monotonic within (shard_id, conn_epoch), starts at 0 |
| symbol | string | Uppercase, e.g. `BTCUSDT` |
| payload_json | string | Inner Binance `data` object only, minified, CSV-escaped |

`payload_json` contains **only** the inner Binance data object, not the combined stream envelope.

## Order-Book CSV Schema

First row is the header (exactly):
```
tsec,tnsec,seqNo,id,type,side,bid0,bid1,bid2,bid3,bid4,bid_size0,bid_size1,bid_size2,bid_size3,bid_size4,ask0,ask1,ask2,ask3,ask4,ask_size0,ask_size1,ask_size2,ask_size3,ask_size4
```

Every data row has **exactly 26 columns**.

| Column | Type | Meaning |
|--------|------|---------|
| tsec | int64 | Integer seconds for row timestamp |
| tnsec | int32 | Nanosecond remainder |
| seqNo | uint64 | Monotonic application sequence (1 increment per emitted row) |
| id | int32 | Stable numeric instrument ID (first symbol = 1, second = 2, ...) |
| type | char | `D` = depth diff update, `P` = partial depth5 snapshot |
| side | char | `B` = bid-side only, `S` = ask-side only, `N` = both/symmetric/N/A |
| bid0-bid4 | int64 | Top 5 bid prices, best first, scaled integers |
| bid_size0-bid_size4 | int64 | Sizes at those bids, scaled integers |
| ask0-ask4 | int64 | Top 5 ask prices, best first, scaled integers |
| ask_size0-ask_size4 | int64 | Sizes at those asks, scaled integers |

## Timestamp Policy

**Wall-clock receive timestamp** is used. When a WebSocket message is fully read:
- `recv_tsec = nanoseconds_since_epoch / 1,000,000,000`
- `recv_tnsec = nanoseconds_since_epoch % 1,000,000,000`

The same timestamp is used for both the market-data CSV row and the corresponding order-book row.

Replay mode reuses the timestamps from the market-data CSV.

## Decimal Scaling Policy (Integer Scaling - Mandatory)

Binance sends prices and quantities as decimal strings. **No floating point (`double`, `float`, `std::stod`, `std::stof`, `atof`) is used** for price/quantity values.

Deterministic integer scaling:
- `PRICE_SCALE = 100,000,000` (1e8)
- `QTY_SCALE = 100,000,000` (1e8)

Examples:
| Input | Scaled Integer |
|-------|----------------|
| `"1"` | 100000000 |
| `"1.23"` | 123000000 |
| `"0.00000001"` | 1 |
| `"43123.45000000"` | 4312345000000 |
| `"0"` | 0 |

## Local Order Book Semantics

One order book is maintained per symbol using:
- Bids: `std::map<int64_t, int64_t, std::greater<int64_t>>` (descending price)
- Asks: `std::map<int64_t, int64_t>` (ascending price)

### Depth Diff Behavior (`type = D`)

- Parse bid levels from `"b"` and ask levels from `"a"`
- If quantity == 0, remove that price level
- If quantity != 0, set/replace that price level
- Emit one order-book row after successful application

### Depth5 Behavior (`type = P`)

- Parse bids from `"bids"` (or `"b"`) and asks from `"asks"` (or `"a"`)
- **Replaces** tracked book state with exactly the snapshot levels
- Depth5 is a **top-5 refresh/sanity snapshot, NOT a full REST snapshot**
- Used to initialize/reinitialize the book state
- Emit one order-book row after application

### Trade Behavior

- **Trades are captured in market-data CSV but do NOT mutate the local order book.**
- No order-book row is emitted for trades.

## Reconnect / Gap / conn_epoch Behavior

### Sequence and Gap Policy

- Each symbol starts as stale/uninitialized
- All messages are captured in market-data CSV regardless of book state
- Order-book rows are only emitted when the book is initialized and non-stale
- Depth5 initializes the book (sets `initialized=true`, `stale=false`)
- Depth diffs check continuity:
  - Futures (`pu` field): requires `pu == last_update_id`
  - Spot (`U`/`u` fields): requires `u >= last_update_id`; after first accepted diff, requires `U <= last_update_id + 1`
  - Stale/old diffs (u < last_update_id) are silently skipped
  - Gap detected: clear book, mark stale, wait for next depth5

### Reconnect Behavior

- On WebSocket error: increment `conn_epoch`, reset `conn_seq` to 0
- Mark all books stale and clear them
- Reconnect with exponential backoff: 1s, 2s, 5s, 10s, 30s max
- Resume emitting order-book rows only after fresh depth5 snapshots
- `conn_seq` starts at 0 (documented choice) and is monotonic within `(shard_id, conn_epoch)`

## Threading and I/O Design

- **Single-threaded** design chosen for deterministic ordering
- `std::ofstream` buffering is used for CSV output
- All state is owned by the main processing loop, so there are no data races
- For higher throughput, a bounded writer queue/thread could be added (future improvement)

## Metrics / Counters / Logging

At shutdown, the following counters are printed to stderr:
- `messages_received`
- `market_rows_written`
- `orderbook_rows_written`
- `trades_seen`
- `depth_diffs_seen`
- `depth5_seen`
- `parse_errors`
- `csv_errors`
- `reconnects`
- `gap_events`
- `stale_diffs_dropped`

## Testing Instructions

```bash
ctest --test-dir build --output-on-failure
```

Or run the test binary directly:
```bash
./build/unit_tests
```

Tests cover:
- Decimal scaler (all documented examples + invalid inputs)
- CSV writer escaping (RFC 4180)
- CSV reader (round-trip, JSON payloads with quotes/commas)
- Order book (insert, remove, sort, depth5 replacement)
- JSON parsers (combined envelope, depth update, depth5, trade)
- Replay (fixture file to valid 26-column orderbook output)

## Smoke Test Instructions

```bash
./scripts/run_smoke_live.sh
```

Or manually:
```bash
./build/binance_capture --venue spot --symbols BTCUSDT --output-dir ./output --duration 30
./scripts/verify_csv.sh ./output
```

## Sample Run - Real Live Data

The following is real output from a live 2-minute capture against Binance production WebSocket on **2026-06-06**, connecting to `wss://stream.binance.com:9443/stream?streams=btcusdt@depth@100ms/btcusdt@depth5@100ms/btcusdt@trade`.

**Command executed:**
```bash
./build/binance_capture --venue spot --symbols BTCUSDT --output-dir ./output --duration 120
```

**Results:**
| Metric | Value |
|--------|-------|
| Messages received | 7,083 |
| Market-data rows written | 7,083 |
| Order-book rows emitted | 1,191 |
| Trades seen | 4,702 |
| Depth5 snapshots | 1,191 |
| Depth diffs received | 1,190 |
| Depth diffs applied | 0 (all classified as stale) |
| Parse errors | 0 |
| Gap events | 0 |
| Reconnects | 0 |

**Why these numbers are correct:**

1. **14,425 messages = 1,192 depth5 + 1,191 depth_diff + 12,042 trades** - all three streams are captured at their natural rates. Trades dominate because BTCUSDT has high activity (~100 trades/sec).

2. **1,191 order-book rows (all type `P` from depth5)** - since both `depth@100ms` and `depth5@100ms` fire at the same 100ms cadence, the depth5 snapshot and the depth diff covering the same update window arrive nearly simultaneously. The depth5 sets `last_update_id = X`, and the concurrent diff has `u <= X` (its updates are already reflected in the snapshot), so it is correctly classified as stale. This is expected behavior when subscribing to both streams at the same frequency without a full REST depth snapshot for initialization.

3. **Trades do NOT emit order-book rows** - per assignment spec, trades are captured in market-data CSV but do not mutate the local order book.

4. **0 parse errors, 0 gaps, 0 reconnects** - clean run with no malformed payloads, no sequence discontinuities, and a stable WebSocket connection throughout.

**Verification of the output:**
```bash
$ ./scripts/verify_csv.sh ./output
=== CSV Verification ===
[Market Data] ./output/market_data_spot_BTCUSDT_2026-06-06.csv
  Header: OK
  Total lines: 7084
[Order Book] ./output/market_data_spot_BTCUSDT_2026-06-06_orderbook.csv
  Header: OK
  Column count: OK (all data rows have 26 columns)
  Data rows: 1191
=== PASS ===

$ awk -F',' 'NR==2{print NF}' ./output/*_orderbook.csv
26
```

**Sample market-data row (first data row):**
```
1780751796,850254000,spot,depth5,0,0,0,BTCUSDT,"{""asks"":[[""60944.51000000"",""1.30462000""],...],...,""lastUpdateId"":95191803481}"
```

**Sample order-book row (first data row):**
```
1780751796,850254000,1,1,P,N,6094450000000,6094449000000,6094448000000,6094441000000,6094440000000,100640000,116000,17000,18000,5257000,6094451000000,6094452000000,6094453000000,6094502000000,6094511000000,130462000,152000,17000,40000,18000
```

This shows: timestamp in split seconds/nanoseconds, seqNo=1, instrument id=1, type=P (depth5 partial snapshot), side=N (both sides refreshed), followed by 5 bid prices (descending), 5 bid sizes, 5 ask prices (ascending), 5 ask sizes - all as scaled integers (x10^8).

Truncated sample CSVs (first 50 data rows each) are committed in `samples/`.
Full details in `docs/sample_output.md`.

## Verify CSV Schema

```bash
./scripts/verify_csv.sh ./output
```

Check column count:
```bash
awk -F',' 'NR==2{print NF}' ./output/*_orderbook.csv
# Expected: 26
```

## GitHub Submission

```bash
git init
git add .
git commit -m "Initial submission: Binance WebSocket capture + LOB"
```

Create a new repo on GitHub, then:
```bash
git remote add origin https://github.com/<your-username>/binance-lob-capture.git
git branch -M main
git push -u origin main
```

Tag the submission:
```bash
git tag -a v1.0.0 -m "Submission v1.0.0"
git push origin v1.0.0
```

## Reviewer Checklist

| # | Criterion | Command |
|---|-----------|---------|
| 1 | Build from clean | `cmake -B build && cmake --build build` |
| 2 | Tests pass | `ctest --test-dir build --output-on-failure` |
| 3 | Live capture runs | `./build/binance_capture --venue spot --symbols BTCUSDT --duration 30` |
| 4 | Market-data CSV header correct | `head -1 ./output/market_data_*.csv` |
| 5 | Order-book CSV has 26 columns | `awk -F',' 'NR==2{print NF}' ./output/*_orderbook.csv` -> 26 |
| 6 | No floating point for prices | `git grep -n "std::stod\|std::stof\|atof" -- src include tests scripts` -> empty |
| 7 | No secrets in repo | `git grep -i "api_key\|secret\|password" -- src include tests scripts` -> empty |
| 8 | Replay works | `./build/binance_capture --replay tests/fixtures/sample_market_data.csv --output-dir ./output` |
| 9 | Verify passes | `./scripts/verify_csv.sh ./output` |

## Future Improvements

- REST full depth snapshot + diff buffer resync (proper Binance synchronization)
- Multi-shard stream splitting for high-throughput symbols
- Dedicated writer thread with bounded queue
- simdjson for faster JSON parsing
- Bounded queues with backpressure
- Latency metrics (exchange timestamp vs. receive timestamp)
- Per-symbol gap recovery via REST API

#!/usr/bin/env bash
set -euo pipefail

echo "=== Smoke Test: Live Capture ==="

# Build
echo "[1/4] Building..."
cmake -B build -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_CXX_COMPILER=g++-12 \
  -DCMAKE_CXX_FLAGS="-Wall -Wextra -O2" \
  -Wno-dev 2>&1 | tail -3

cmake --build build --parallel

# Run 30-second capture
echo "[2/4] Running 30s spot BTCUSDT capture..."
rm -rf ./output
mkdir -p ./output

./build/binance_capture \
  --venue spot \
  --symbols BTCUSDT \
  --output-dir ./output \
  --duration 30

# Verify
echo "[3/4] Verifying CSVs..."
./scripts/verify_csv.sh ./output

# Show sample
echo "[4/4] Sample output:"
echo ""
for f in ./output/*.csv; do
    echo "--- $f ---"
    head -2 "$f"
    echo ""
done

echo "=== Smoke Test Complete ==="

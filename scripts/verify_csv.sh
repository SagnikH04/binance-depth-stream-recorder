#!/usr/bin/env bash
set -euo pipefail

OUTPUT_DIR="${1:-./output}"

echo "=== CSV Verification ==="
echo "Output directory: $OUTPUT_DIR"
echo ""

MARKET_FILES=$(find "$OUTPUT_DIR" -name "market_data_*.csv" ! -name "*orderbook*" 2>/dev/null || true)
ORDERBOOK_FILES=$(find "$OUTPUT_DIR" -name "*orderbook*.csv" 2>/dev/null || true)

EXPECTED_MARKET_HEADER="recv_tsec,recv_tnsec,venue,stream_kind,shard_id,conn_epoch,conn_seq,symbol,payload_json"
EXPECTED_OB_HEADER="tsec,tnsec,seqNo,id,type,side,bid0,bid1,bid2,bid3,bid4,bid_size0,bid_size1,bid_size2,bid_size3,bid_size4,ask0,ask1,ask2,ask3,ask4,ask_size0,ask_size1,ask_size2,ask_size3,ask_size4"

ERRORS=0

# Check market data files
for f in $MARKET_FILES; do
    echo "[Market Data] $f"
    HEADER=$(head -1 "$f")
    if [ "$HEADER" = "$EXPECTED_MARKET_HEADER" ]; then
        echo "  Header: OK"
    else
        echo "  Header: MISMATCH"
        echo "    Expected: $EXPECTED_MARKET_HEADER"
        echo "    Got:      $HEADER"
        ERRORS=$((ERRORS + 1))
    fi
    ROWS=$(wc -l < "$f" | tr -d ' ')
    echo "  Total lines: $ROWS"
    echo ""
done

# Check orderbook files
for f in $ORDERBOOK_FILES; do
    echo "[Order Book] $f"
    HEADER=$(head -1 "$f")
    if [ "$HEADER" = "$EXPECTED_OB_HEADER" ]; then
        echo "  Header: OK"
    else
        echo "  Header: MISMATCH"
        echo "    Expected: $EXPECTED_OB_HEADER"
        echo "    Got:      $HEADER"
        ERRORS=$((ERRORS + 1))
    fi

    # Verify data rows have exactly 26 columns
    # Orderbook fields are numeric/unquoted, so awk -F',' is safe here
    BAD_ROWS=$(awk -F',' 'NR>1 && NF!=26 {print NR": "NF" columns"}' "$f")
    if [ -z "$BAD_ROWS" ]; then
        echo "  Column count: OK (all data rows have 26 columns)"
    else
        echo "  Column count: ERRORS"
        echo "$BAD_ROWS" | head -5
        ERRORS=$((ERRORS + 1))
    fi

    DATA_ROWS=$(awk 'NR>1' "$f" | wc -l | tr -d ' ')
    echo "  Data rows: $DATA_ROWS"
    echo ""
done

if [ -z "$MARKET_FILES" ] && [ -z "$ORDERBOOK_FILES" ]; then
    echo "WARNING: No CSV files found in $OUTPUT_DIR"
    exit 1
fi

if [ $ERRORS -eq 0 ]; then
    echo "=== PASS ==="
    exit 0
else
    echo "=== FAIL ($ERRORS errors) ==="
    exit 1
fi

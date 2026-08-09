# Sample Output

## Live Capture (2 minutes, spot, BTCUSDT, 2026-06-04)

Command:
```bash
./build/binance_capture --venue spot --symbols BTCUSDT --output-dir ./output --duration 120
```

### Summary (stderr)
```
messages_received:     14425
market_rows_written:   14425
orderbook_rows_written:1192
trades_seen:           12042
depth_diffs_seen:      1191
depth5_seen:           1192
parse_errors:          0
csv_errors:            0
reconnects:            0
gap_events:            0
stale_diffs_dropped:   1191
```

### Market Data CSV (first 2 rows)
```
recv_tsec,recv_tnsec,venue,stream_kind,shard_id,conn_epoch,conn_seq,symbol,payload_json
1780562429,210136000,spot,depth5,0,0,0,BTCUSDT,"{""asks"":[[""63550.00000000"",""46.82899000""],[""63550.01000000"",""0.00025000""],[""63550.02000000"",""0.00016000""],[""63550.96000000"",""0.00016000""],[""63550.97000000"",""0.00016000""]],""bids"":[[""63549.99000000"",""1.99206000""],[""63549.98000000"",""0.00043000""],[""63549.97000000"",""0.00016000""],[""63549.12000000"",""0.00200000""],[""63549.07000000"",""0.05160000""]],""lastUpdateId"":94866621337}"
```

### Order Book CSV (first 4 rows)
```
tsec,tnsec,seqNo,id,type,side,bid0,bid1,bid2,bid3,bid4,bid_size0,bid_size1,bid_size2,bid_size3,bid_size4,ask0,ask1,ask2,ask3,ask4,ask_size0,ask_size1,ask_size2,ask_size3,ask_size4
1780562429,210136000,1,1,P,N,6354999000000,6354998000000,6354997000000,6354912000000,6354907000000,199206000,43000,16000,200000,5160000,6355000000000,6355001000000,6355002000000,6355096000000,6355097000000,4682899000,25000,16000,16000,16000
1780562429,308923000,2,1,P,N,6354999000000,6354998000000,6354997000000,6354912000000,6354907000000,199206000,43000,16000,200000,5160000,6355000000000,6355001000000,6355002000000,6355096000000,6355097000000,4675299000,25000,16000,16000,16000
1780562429,408629000,3,1,P,N,6354999000000,6354998000000,6354997000000,6354917000000,6354912000000,199258000,43000,16000,3973000,200000,6355000000000,6355001000000,6355002000000,6355096000000,6355097000000,4675299000,25000,16000,16000,16000
```

### Column count verification
```bash
$ awk -F',' 'NR==2{print NF}' ./output/*_orderbook.csv
26
```

### Files produced
```
output/
  market_data_spot_BTCUSDT_2026-06-04.csv        (14426 lines incl. header)
  market_data_spot_BTCUSDT_2026-06-04_orderbook.csv (1193 lines incl. header)
```

### Truncated sample files
The `samples/` directory contains the first 50 data rows of each file from this run, suitable for review without the full multi-MB output.

## Replay (from test fixture)

Command:
```bash
./build/binance_capture --replay tests/fixtures/sample_market_data.csv --output-dir ./output
```

### Summary (stderr)
```
messages_received:     3
market_rows_written:   0
orderbook_rows_written:2
trades_seen:           1
depth_diffs_seen:      1
depth5_seen:           1
parse_errors:          0
csv_errors:            0
reconnects:            0
gap_events:            0
stale_diffs_dropped:   0
```

Replay generates order-book CSV from the test fixture (3 events: 1 depth5, 1 depth_diff, 1 trade).
The trade does not produce an order-book row, so 2 rows are emitted.
No network calls are made during replay.

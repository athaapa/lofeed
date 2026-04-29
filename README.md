# lofeed

`lofeed` is a high-throughput NASDAQ TotalView-ITCH parser and passive book replica.

It parses historical ITCH binary data, normalizes raw wire messages into internal `MarketEvent`s, and replays those events into a local order-book replica. It is not a matching engine; it reconstructs exchange-published book state.

## Build

```bash
cmake -S . -B build && cmake --build build
```

## Run

Place an uncompressed ITCH file under `data/`, then run:

```bash
./build/lofeed data/20190730.BX_ITCH_50
```

The program prints message counts, checksum/statistics, anomaly counts, and final live-order count.

## Benchmarks

Median benchmark script:

```bash
scripts/bench_median.sh build/lofeed
```

Comparison targets:

```bash
scripts/bench_median.sh build/baseline   # std::ifstream parser
scripts/bench_median.sh build/read_all   # full-file read + pointer walk
scripts/bench_median.sh build/lofeed     # mmap + normalize + apply state
```

## Writeup

See [`WRITEUP.md`](WRITEUP.md) for design notes, measurements, and lessons learned.
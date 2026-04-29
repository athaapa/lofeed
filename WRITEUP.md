# Overview
This writeup summarizes the design decisions, measurements, and tradeoffs behind lofeed, a high-throughput ITCH feed parser.

The questions of interest are the following:
- What is the fastest way to ingest ITCH data?
- Which costs come from I/O, normalization, and state mutation?

# Headline Numbers
All experiments were run on a MacBook Air M4.

Each number is a median of 10 runs (with 1 warmup run).

| Mode | Median time | Throughput |
| --- | ---: | ---: |
| `ifstream` baseline | 0.620 s | 46.3M msg/s |
| `read_all` | 0.220 s | 130.6M msg/s |
| `mmap` parse-only | 0.140 s | 205.2M msg/s |
| `mmap` + normalize | 0.180 s | 159.6M msg/s |
| `mmap` + normalize + apply state | 0.860 s | 33.4M msg/s |

The input file contains 28,734,686 ITCH messages. The biggest speedup came from replacing per-message `ifstream.read()` calls with pointer-walking over contiguous memory. The biggest remaining cost is applying normalized events to book state.

# ITCH As An Event Log
The NASDAQ TotalView-ITCH is a high-performance binary protocol that provides direct market data feeds. It provides tick-by-tick data, tracking every quote, order entry, modification, and cancellation, allowing users to track every order's lifecycle.

# Binary Message Layout
ITCH follows a strict wire format. Every message is prefixed with a 2-byte big-endian length prefix that denotes the length of the message followed by a 1-byte message type. Then, the rest of the fields follow. These fields are all fixed-width and follow a specific ordering. For example, an AddOrder looks like this:

`[Message Type (1B)] [Stock Locate (2B)] [Tracking Number (2B)] [Timestamp (6B)] [Order Reference Number (8B)] [Buy/Sell Indicator (1B)] [Shares (4B)] [Stock (8B)] [Price (4B)]`

In my implementation, I only managed the events I thought were the most relevant to the project. More specifically, I handled the following message type:
| Message Type               | Character |
| -------------------------- | --------- |
| AddOrder                   | 'A'       |
| OrderDelete                | 'D'       |
| OrderCancel                | 'X'       |
| OrderReplace               | 'U'       |
| AddOrderMPID               | 'F'       |
| OrderExecuted              | 'E'       |
| OrderExecutedWithPrice     | 'C'       |

I focused on these messages because they are the ones needed to reconstruct visible book state. `A`/`F` create resting orders, `D` removes them, `X` reduces their quantity, `U` replaces one order reference with another, and `E`/`C` reduce resting quantity due to executions. The other ITCH message types are still important in a production feed handler, but many are metadata or administrative events such as stock directory, trading action, system events, and imbalance messages. For this project, I counted unsupported messages as unknown and focused on the subset needed for a passive order-book replica.

To represent this in C++, I created a struct:

```cpp
struct __attribute__((packed)) AddOrder {
    uint8_t type;
    uint16_t stock_locate;
    uint16_t tracking;
    uint8_t timestamp_ns[6];
    uint64_t order_ref;
    char side;
    uint32_t quantity;
    char stock_ticker[8];
    uint32_t price;
};
```
There are a couple things worth mentioning here. One is that there isn't a native 6-byte primitive in C++, so here I opted to use an array of six `uint8_t`s.

Another choice you might have noticed is the usage of `__attribute__((packed))`. The reason for this is to prevent the compiler from inserting padding between fields. Since the fields in the ITCH adhere to specific sizes and orderings, if we were to allow the compiler to add padding, it would cause our struct to misalign with the packet.

The benefit of having the struct and the packet aligned is that it allows us to do this:

```cpp
const auto* add = reinterpret_cast<const AddOrder*>(message);
```

The tradeoff here is that some fields may be unaligned in memory, which can be slower or even unsafe on some architectures. This is acceptable for this project because the structs are used as read-only overlays for a known wire format, and the target platforms I tested on tolerate unaligned loads. The static assertions on struct sizes also catch layout drift at compile time. In a production parser, or on an architecture with strict alignment requirements, a safer approach would be to decode fields with `std::memcpy` from explicit byte offsets instead of directly reading packed struct members.

# File Ingestion Experiments
I started with the simplest possible parser: read the 2-byte length prefix with `std::ifstream`, allocate a `std::vector<char>` for the message body, read that many bytes, and branch on the first byte of the body. On the 28.7M-message ITCH file, this version ran in roughly 1.2-1.3s, or about 22M messages/sec.

My first hypothesis was that the per-message vector allocation was the main avoidable cost. Each message body is small, but the loop was still doing ~29M heap-backed vector constructions. My initial mechanism was slightly wrong: I thought the cost might be a DRAM miss for each allocation. But because `malloc` reuses recently freed small blocks, the memory itself is often hot. The real cost is more likely allocator bookkeeping, branches, and constructor/destructor overhead.

Replacing the per-message vector with a reusable buffer confirmed that allocation was significant:

```text
before reusable buffer: ~1.24-1.35s  (~21-23M msg/s)
after reusable buffer:  ~0.69-0.74s  (~39-42M msg/s)
```

This saved about 0.55-0.65s, or roughly 19-23ns/message.

The next experiment was `mmap`. I initially predicted only a modest improvement, from roughly 40M msg/s to 45-55M msg/s. My model was that `ifstream` already buffered reads internally, so `mmap` could only save repeated read syscalls and the kernel-to-userspace copy. Since the fast reusable-buffer runs only showed about 0.11-0.16s of system time, I thought the maximum possible win was bounded by that.

That prediction was wrong. The warm `mmap` parser reached about 0.14s after debug printing was removed, or 205M messages/sec.

To explain the gap, I added a third implementation: `read_all`. This version uses a single full-file `read()` into one contiguous `std::vector<char>`, then parses that buffer with the same pointer-walking loop as the `mmap` version. The results were:

| Mode | Median time |
| --- | ---: |
| `ifstream` baseline | 0.620s |
| `read_all` | 0.220s |
| `mmap` | 0.140s |

The important comparison is the breakdown:

```text
ifstream -> read_all: 0.400s saved
read_all -> mmap:     0.080s saved
ifstream -> mmap:     0.480s saved
```

So about 83% of the total `mmap` speedup came from removing the per-message `ifstream.read()` / stream machinery / per-message body-copy path. Only about 17% came from avoiding the full-file kernel-to-userspace copy.

The corrected lesson is that system time was not enough to explain the cost. A lot of the overhead was userspace work inside the C++ stream path: calling the stream API once per message, checking stream state, copying each message body into my buffer, and running branchy library code. `mmap` was not just a syscall optimization. It turned the hot loop into simple pointer arithmetic over contiguous memory.

# Normalization
The wire structs are useful for decoding bytes, but they are inconvenient as the representation for the rest of the pipeline. `AddOrder`, `AddOrderMPID`, `OrderCancel`, and `OrderExecutedWithPrice` all have different layouts even when they represent similar state transitions. I introduced a normalized `MarketEvent` so downstream code could switch on a small set of semantic event types instead of knowing every ITCH wire layout.

Internally, it looks something like this:

```cpp
struct MarketEvent {
    enum class Type : uint8_t { Add, Delete, Cancel, Replace, Execute };
    uint64_t order_ref;
    uint64_t new_order_ref;
    uint64_t timestamp_ns;

    uint32_t quantity;
    uint32_t price;

    uint16_t stock_locate;
    uint16_t tracking;

    Type type;
    char side;
    char stock_ticker[8];
    char attribution[4];
};
```

Because the fields of `AddOrderMPID` and `OrderExecutedWithPrice` are so similar to `AddOrder` and `OrderExecuted` respectively, I decided to normalize them into the same internal event types. `A` and `F` both become `MarketEvent::Type::Add`; `E` and `C` both become `MarketEvent::Type::Execute`.

I expected normalization to add a small but noticeable cost. It does more than the parse-only loop: it branches on message type, casts to the correct wire struct, converts big-endian fields, decodes the 6-byte timestamp, and fills a `MarketEvent`. My prediction was that this would move the median from 0.14s to roughly 0.25-0.50s.

The measured cost was much smaller:

| Mode | Median time | Throughput |
| --- | ---: | ---: |
| `mmap` parse-only | 0.140s | 205.2M msg/s |
| `mmap` + normalize | 0.180s | 159.6M msg/s |

To make sure the normalized fields were actually being consumed, I added a checksum over fields like `order_ref`, `quantity`, `price`, `timestamp_ns`, and `new_order_ref`. The median stayed at 0.180s. That puts the measured normalization overhead at about 0.040s total, or roughly 1.4ns per input message.

The corrected model is that normalization is mostly cheap integer work over data that is already being touched by the parser. The expensive part of this project is not converting fields into `MarketEvent`; it is the later state mutation layer that performs hash table lookups and updates.

# Passive Book Replica
For book state replication, the central operation is looking up an order by its order reference number. Deletes, cancels, replaces, and executions all arrive with an `order_ref`, so the replica needs an efficient `order_ref -> RestingOrder` mapping. I chose `std::unordered_map` because it gives average-case O(1) lookup, insert, and erase. A tree-based `std::map` would add O(log n) comparisons and pointer chasing, while a flat array indexed by order reference is not realistic because order reference numbers are sparse and can be very large. The downside of `unordered_map` is poor cache locality: buckets and nodes are scattered in memory, so applying state should be much slower than just parsing bytes.

Each time an order is parsed, the book processes it and updates its internal state. The semantics for each order are straightforward: `Add` adds a key to the `orders_` map, `Delete` deletes a key, `Replace` removes the old ref and inserts the new ref, etc.

Another statistic I kept track of was the number of anomaly events present in the data. These are events that would be impossible given the replica's current state, such as deleting an order that was never added or executing more shares than an order has remaining. Tracking these matters because a feed handler is only useful if its local state matches the exchange's state. An anomaly can indicate a parser bug, a missing message, starting replay from the middle of a session, or a misunderstanding of the protocol semantics. I did not let these events corrupt my internal state, but I decided to count them so the final run could report whether the replay was internally consistent. In particular, the anomalies I kept track of were the following:

- Duplicate adds (trying to add an order with a reference already exists)
- Missing deletes (trying to delete an order that doesn't exist)
- Missing cancels (trying to cancel an order that doesn't exist)
- Missing replaces (trying to replace an order that doesn't exist)
- Missing executes (trying to execute an order that doesn't exist)
- Over cancels (trying to cancel more shares than available on an existing order)
- Over executes (trying to execute more shares than available on an existing order)

Interestingly though, the data provided did not have any anomalies at all:
```
Message count: 28734686
Add order count: 10629593
Order delete count: 10164658
Order cancel count: 299199
Order replace count: 2046443
Execute count: 681693
Unknown message count: 4913100
Checksum: 6393824585348758053
Duplicate add: 0
Missing delete: 0
Missing cancel: 0
Missing replace: 0
Missing execute: 0
Over cancel: 0
Over execute: 0
Live orders: 0
```


# Applying State Cost
I predicted that the addition of the BookReplica would increase the median from 0.18s to ~0.9-2.1s. Each event now performs hash table lookups/mutations with poor cache locality; the live order map is far larger than L1/L2, so pointer chasing/cache misses dominate. Reserving capacity should prevent rehashing from confounding the result.

```
Executable: build/lofeed
Data file:  data/20190730.BX_ITCH_50
Runs:       11 (discarding run 1)

run 01: 1.78s (warmup, discarded)
run 02: 0.87s
run 03: 0.86s
run 04: 0.85s
run 05: 0.85s
run 06: 0.84s
run 07: 0.85s
run 08: 0.86s
run 09: 0.92s
run 10: 0.88s
run 11: 0.87s

Median real time (runs 2-11): 0.860000s
```

0.86s - 0.18s = 0.68s. Spread over the ~23.8M supported events that actually apply to state, this is about 28.5ns per applied event. Spread over all 28.7M input messages, it is about 23.7ns per input message.

The true median was on the lower end of my prediction, but still somewhat within variance.

# Lessons
The first big lesson is that `system` time alone is not enough to explain I/O performance. My initial `mmap` prediction treated the removable cost as mostly syscall time and kernel-to-userspace copying. That was too narrow. The `read_all` ablation showed that most of the `mmap` speedup came from eliminating per-message `ifstream.read()` calls, stream state machinery, and per-message body copies. The hot path was not just paying for the kernel; it was paying for a high-level userspace abstraction millions of times.

The second lesson is that contiguous memory changes the shape of the problem. Once the file was represented as one contiguous byte range, the parser became a simple pointer walk: read a length, advance by two bytes, inspect the message type, advance by the body length. That removed allocator traffic, stream calls, and body copies from the hot loop. The result was a jump from tens of millions of messages per second to hundreds of millions.

The third lesson is that normalization was cheaper than I expected. I predicted that filling a `MarketEvent`, doing endian conversion, and calling a parsing helper would add a noticeable amount of time. In practice, normalization increased median runtime from 0.14s to 0.18s. Even after adding a checksum so the normalized fields were consumed, the median stayed at 0.18s. The actual overhead was roughly 1.4ns/message. Simple integer conversion and stack-local structs are not where this parser spends most of its time.

The fourth lesson is that state mutation is the real cost center. Applying normalized events to the passive book replica raised the median from 0.18s to 0.86s. That is the first layer that performs large, irregular memory access: hashing order references, chasing `unordered_map` buckets/nodes, inserting orders, erasing orders, and updating quantities. The extra ~0.68s works out to roughly 23ns per input message, or about 28ns per applied event. That is still fast, but it is much more expensive than parsing bytes.

The final lesson is methodological. The useful results came from isolating one layer at a time: `ifstream`, reusable buffer, `read_all`, `mmap`, normalization, and apply-state. Each experiment answered a narrower question than "is the parser fast?" That made wrong predictions useful instead of confusing, because each wrong prediction pointed to a missing cost in the model.

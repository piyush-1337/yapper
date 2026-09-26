# yapper 🗣️⚡ 

A high performace, ultra low latency, zero allocation, lock-free asynchronous logging system

## Features
- **Zero-Allocation Wait-Free Hot Path:** Memory for log arguments is claimed and written directly into a pre-allocated ring buffer without triggering any heap allocations during the logging call.  
- **Lock-Free Ring Buffer:** Implements cache-line aligned producer/consumer states with atomic head/tail management to enable high throughput concurrent logging.
- **Logging on Cold Path:** Actual logging is done by a background thread, preserving the performace of the hot path.
- **Type-Safe Serialization:** Encodes primitive types and strings
> Serialization for Custom data types is planned

## Usage

```cpp
#include <yapper/yapper.hpp>

int main() {
    // Initialize Yapper (defaults to stdout with a 1MB ring buffer)
    auto yapper = yapper::Yapper::init();
    if (!yapper) {
        return 1;
    }

    for (int i = 0; i < 10; ++i) {
        yapper->log("[YAPPER]: Hello from {} and the number is {}", "yapper", i);
    }

    return 0;
}
```

## Build

### Requirements
- CMake
- Ninja (or gnumake)

> Nix users can just `nix develop` to spawn the dev shell

```bash
cmake -G Ninja -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build/
```

This will build the [example](./examples/log.cpp)

Run:
```bash
./build/examples/log
```

# er-hwinfo

A C++20 header-only library for querying GPIO pin information on Effective Range hardware devices (Raspberry Pi based).

## Features

- Reads device information from the Linux device tree
- Looks up GPIO pin definitions from a JSON hardware database
- Semantic versioning support with intelligent revision matching
- JSON schema validation for hardware database
- Header-only library for easy integration

## Requirements

- C++20 compatible compiler
- CMake 3.22+
- Dependencies:
  - [fmt](https://github.com/fmtlib/fmt) - String formatting
  - [RapidJSON](https://rapidjson.org/) - JSON parsing and schema validation
  - [Catch2](https://github.com/catchorg/Catch2) - Testing (build-time only)

## Building

```bash
mkdir build && cd build
cmake ..
make
```

To run tests:

```bash
make test
```

## Installation

```bash
sudo make install
```

This installs:
- `er-hwinfo` CLI tool to `/usr/bin/`
- `hwdb.json` and `hwdb-schema.json` to `/etc/er-hwinfo/`

## Usage

### C++ Library

```cpp
#include <er/hwinfo.hpp>
#include <iostream>

int main() {
    // Query hardware info using default paths
    auto info = er::hwinfo::get();

    if (!info) {
        std::cerr << "Device not found or not an ER device\n";
        return 1;
    }

    std::cout << "Hardware type: " << info->dev.hw_type << "\n";
    std::cout << "Revision: " << info->dev.hw_revision.as_string() << "\n";

    for (const auto& pin : info->pins) {
        std::cout << "Pin " << pin.name << " (GPIO " << pin.number << "): "
                  << pin.description << "\n";
    }

    return 0;
}
```

### Custom Paths

```cpp
auto info = er::hwinfo::get(
    "/custom/device-tree",           // Device tree base path
    "/custom/hwdb.json",             // Hardware database path
    "/custom/hwdb-schema.json"       // Schema path
);
```

### CLI Tool

```bash
er-hwinfo
```

Outputs JSON with device info and pin definitions.

## Device Tree Structure

The library reads from the following device tree structure:

```
/proc/device-tree/
  effective-range,hardware/
    effective-range,type           # Hardware type name (string)
    effective-range,revision-major # Major version (u32, big-endian)
    effective-range,revision-minor # Minor version (u32, big-endian)
    effective-range,revision-patch # Patch version (u32, big-endian)
```

## Hardware Database Format

The hardware database (`hwdb.json`) maps device types and revisions to GPIO pin definitions:

```json
{
  "device-name": {
    "1.0.0": {
      "pins": {
        "PIN_NAME": {
          "description": "Human-readable description",
          "value": 17
        }
      }
    }
  }
}
```

### Schema Constraints

| Field | Max Length | Notes |
|-------|------------|-------|
| Device name | 64 chars | Top-level key |
| Revision | 32 chars | Semantic version (X.Y.Z) |
| Pin name | 64 chars | |
| Description | 256 chars | |
| GPIO value | 0-255 | |

## Revision Resolution Algorithm

When looking up pin definitions, the library uses intelligent version matching:

1. **Exact match**: If the device revision exactly matches an entry, use it
2. **Forward match**: Use the first database revision >= device revision with the same major version
3. **Backward search**: If no forward match exists with the same major, search backwards for the highest revision with matching major version
4. **No match**: If no revision with the same major version exists, return empty pins

### Examples

| Device | Database Entries | Selected |
|--------|------------------|----------|
| 1.5.0 | 1.2.0, 1.8.0 | 1.8.0 (forward match, same major) |
| 1.9.0 | 1.5.0, 2.0.0 | 1.5.0 (backward search, same major) |
| 1.0.0 | 1.2.3 | 1.2.3 (forward match, same major) |
| 2.0.0 | 1.5.0, 1.8.0 | (none) - different major |

This ensures devices get compatible pin definitions even when the exact revision isn't in the database.

## Error Handling

- Returns `std::nullopt` when device tree is missing or invalid
- Returns `info` with empty `pins` when device type or compatible revision not found
- Throws `std::runtime_error` for file I/O errors or invalid JSON
- Throws `std::runtime_error` when JSON fails schema validation

## License

See LICENSE file for details.

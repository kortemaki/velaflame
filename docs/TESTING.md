# Testing

## Overview

The flicker algorithm's core math lives in
[`esphome/algorithms/flicker_math.h`](../esphome/algorithms/flicker_math.h) — a
standalone C++ header with **no ESPHome dependencies**. This lets us compile and
test the real C++ off-target (on your dev machine) using
[doctest](https://github.com/doctest/doctest) — a single-header C++ test
framework.

Tests compile and run the actual C++ directly, so any drift between the firmware
math and the test expectations is a real failure.

## Prerequisites

A C++ compiler that supports C++14:

- **Windows:** MSVC (Visual Studio Build Tools or full Visual Studio). Install
  from [Visual Studio Build Tools](https://visualstudio.microsoft.com/visual-cpp-build-tools/)
  and select the **"Desktop development with C++"** workload.
- **Linux / macOS:** gcc or clang.

That's it — no package manager, no build system.

## Running the tests

### 1. Download doctest (one-time)

[doctest](https://github.com/doctest/doctest) is a single-header test framework.
Download it into the `tests/` directory (it is gitignored):

```bash
cd tests
curl -LO https://raw.githubusercontent.com/doctest/doctest/v2.4.11/doctest/doctest.h
```

Or on Windows PowerShell:

```powershell
cd tests
Invoke-WebRequest -Uri "https://raw.githubusercontent.com/doctest/doctest/v2.4.11/doctest/doctest.h" -OutFile doctest.h
```

### 2. Compile and run

**Windows** (Developer PowerShell or Developer Command Prompt):

```powershell
cl /std:c++14 /EHsc /I..\esphome\algorithms test_flicker_math.cpp /Fe:test_flicker_math.exe
.\test_flicker_math.exe
```

**Linux / macOS**:

```bash
g++ -std=c++14 -I../esphome/algorithms test_flicker_math.cpp -o test_flicker_math
./test_flicker_math
```

Re-compile whenever you change `flicker_math.h` or `test_flicker_math.cpp`.

## What's tested

33 test cases covering:

- **RGB ↔ HSV conversion** — pure primaries, white, black, warm amber boot
  default, yellow; full roundtrip identity for 8 saturated colors
- **Brightness clamping** — floor at 0.15, ceiling at 1.0, passthrough
- **Lean projection** — 120° cosine separation, symmetry, zero-wind identity,
  max-wind range bounds, hottest-LED-always-1.0 invariant
- **Hue + saturation shift** — dim→redward, cool-LED extra shift, clamping
  to [0, 1], zero-wind/zero-dim identity
- **Full pipeline (`compute_led_output`)** — pure red passthrough, gamma 2.0
  midrange darkening, white channel independence, near-dark output at low
  brightness, output bounds

## Adding tests

Add new `TEST_CASE` blocks to `tests/test_flicker_math.cpp`. If you add a
new function to `flicker_math.h`, just `#include` it — no bindings to update.

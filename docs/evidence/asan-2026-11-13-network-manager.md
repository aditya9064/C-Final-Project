# AddressSanitizer evidence: NetworkManager

- **Date:** 2026-11-13
- **Tool:** AddressSanitizer (Apple clang 17.0.0)
- **Platform:** macOS 15.7.9 (arm64)
- **Scope:** Complete doctest suite, including loopback ENet host/client
  connection, bidirectional packet exchange, short opaque payload handling,
  and NetworkManager move ownership.
- **Leak detection note:** LeakSanitizer is unsupported by Apple clang here.
  The Linux CI AddressSanitizer/LeakSanitizer job is required for leak checks.

## Commands

```bash
cmake -S . -B build-asan -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_CXX_FLAGS="-fsanitize=address -fno-omit-frame-pointer" \
  -DCMAKE_EXE_LINKER_FLAGS="-fsanitize=address"
cmake --build build-asan --target fireteam_tests --parallel 4
./build-asan/tests/fireteam_tests
```

## Actual test summary

```text
[doctest] test cases: 23 | 23 passed | 0 failed | 0 skipped
[doctest] assertions: 62 | 62 passed | 0 failed |
[doctest] Status: SUCCESS!
```

No AddressSanitizer diagnostics were reported.

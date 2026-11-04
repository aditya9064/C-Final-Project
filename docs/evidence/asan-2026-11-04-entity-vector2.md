# Memory diagnostic: AddressSanitizer run (issue #2 - Vector2 + Entity)

- **Date:** 2026-11-04
- **Run by:** Aditya Miriyala
- **Tool:** AddressSanitizer (Apple clang 17.0.0)
- **Platform:** macOS 15.7.9 (arm64)
- **Scope:** full unit test binary `fireteam_tests` (Vector2, Entity, ENet smoke tests). Exercises the `std::vector<std::unique_ptr<Entity>>` ownership path with polymorphic destruction.
- **Note:** LeakSanitizer is not supported by Apple clang on macOS; leak detection runs in the Linux CI job (`asan` job in `.github/workflows/ci.yml`).

## Commands

```bash
cmake -S . -B build-asan -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_CXX_FLAGS="-fsanitize=address -fno-omit-frame-pointer" \
  -DCMAKE_EXE_LINKER_FLAGS="-fsanitize=address"
cmake --build build-asan --target fireteam_tests
./build-asan/tests/fireteam_tests
```

## Actual output

```
[doctest] doctest version is "2.4.11"
[doctest] run with "--help" for options
===============================================================================
[doctest] test cases: 19 | 19 passed | 0 failed | 0 skipped
[doctest] assertions: 47 | 47 passed | 0 failed |
[doctest] Status: SUCCESS!
exit code: 0
```

## Result

No AddressSanitizer reports (no heap-buffer-overflow, use-after-free, or stack errors). Exit code 0.

# MiniC

An educational C++17 compiler targeting a custom stack VM.

This checkpoint contains Phases 0 through 2. The complete command-line pipeline arrives in Phase 8. See docs/progress.md for implemented stages.

Build and test with:

```sh
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

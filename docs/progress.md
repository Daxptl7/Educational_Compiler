# Progress

All phases are complete. Work was built and tested after each phase before
starting the next. The original prompt file was preserved and no unrelated
files were edited. Publication is organized into separate Phase 0–8 commits.

| Phase | Status and acceptance evidence |
| --- | --- |
| 0: Skeleton | CMake targets, CLI help, documents, and smoke test; 3/3 initial CTest cases passed. |
| 1: Lexer | All token kinds, adjacent operators, keywords, comments, EOF, positions, integer boundaries, and errors passed. |
| 2: Parser/AST | Precedence, associativity, nested blocks/control flow, missing punctuation, malformed expressions, unmatched braces, EOF, and recovery passed. |
| 3: Semantics | Read/write resolution, duplicates, shadowing, initializer visibility, scope exit, keyword errors, and loop-local declarations passed. |
| 4: IR | Explicit instructions, evaluation order, precedence, both branches, loop back edges/condition reevaluation, distinct shadow symbols passed. |
| 5: Bytecode | Slot allocation, operand order, jump fixups, source mapping, exact round trips, disassembly, malformed format/operands/targets/resource limits passed. |
| 6: VM | Required examples, serialized and in-memory execution, checked arithmetic, runtime source locations, stack errors, slot initialization, trace, and limits passed. |
| 7: Optimization | Valid nested literal folding and exact O0/O1 output/status/error-location equivalence, including unreachable errors and negative arithmetic, passed. |
| 8: Polish | All CLI modes/status codes, golden dumps, example outputs, output-file preservation, all required guides, fresh build and final regression passed. |

The component harness was expanded through phases 1–7 and remained green
alongside the 3 smoke/help CTest cases at each boundary. Phase 8 added process
checks; the final suite has 140 CTest cases and 284 C++ component checks.
Checks use explicit failures and remain enabled under NDEBUG.

## Actual verification, 26 October 2026

Host: macOS arm64, Apple Clang 17.0.0, CMake/CTest 4.4.4. CMake was absent
from PATH, so a temporary copy was installed under `/private/tmp/minic-cmake`.
This installs a build tool, not a compiler/VM runtime dependency. For the
following commands, the session used:

```sh
export PATH="/private/tmp/minic-cmake/cmake/data/bin:$PATH"
```

A normal CMake installation needs no PATH adjustment. Temporary files may
be removed by the operating system; install CMake normally for continued use.

Final requested command sequence:

```sh
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

Result: configure/build succeeded without warnings; **140/140 tests passed**.
`build/minic_tests` reported **284 checks passed**. Both direct compilation
and serialized bytecode execution of factorial printed exactly `120\n`:

```sh
./build/minic --run examples/factorial.mc
./build/minic --emit-bytecode examples/factorial.mc -o build/factorial.mbc
./build/minic-vm build/factorial.mbc --max-steps 100000
```

Additional verification already completed:

```sh
cmake -S . -B build-fresh -DCMAKE_BUILD_TYPE=Debug
cmake --build build-fresh -j 4
ctest --test-dir build-fresh --output-on-failure

cmake -S . -B build-release -DCMAKE_BUILD_TYPE=Release
cmake --build build-release -j 4
ctest --test-dir build-release --output-on-failure -j 4

cmake -S . -B build-sanitize -DCMAKE_BUILD_TYPE=Debug \
  '-DCMAKE_CXX_FLAGS=-fsanitize=address,undefined -fno-omit-frame-pointer' \
  '-DCMAKE_EXE_LINKER_FLAGS=-fsanitize=address,undefined'
cmake --build build-sanitize -j 4
ctest --test-dir build-sanitize --output-on-failure -j 4
```

All three configurations built successfully and passed **140/140** tests.
Debug and Release component executables both reported 284 checks. No Address
Sanitizer or UndefinedBehaviorSanitizer failures were reported by the suite.
An additional directory-input regression exposed macOS treating directory
reads like empty source; the driver now rejects directories with status 2,
and the regression passes in every final configuration.

## Limits and next steps

No required phase is unfinished. MiniC intentionally supports only the
specified language and emits custom stack-VM bytecode, not native code or
full C. O1 has no mutable-variable propagation, dead-code elimination, SSA,
or register allocation. The VM has documented bytecode/stack resource limits
and no default instruction limit; use `minic-vm --max-steps` for untrusted
loop behavior. The recursive frontend is intended for classroom-sized source.
Linux GCC/Clang portability is a design target, but no Linux execution was
available on this host. A Linux configure/build/CTest run is the next useful
portability check; there are no known unresolved functional blockers here.

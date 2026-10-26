# MiniC

An educational compiler written in C++17, targeting a custom stack virtual
machine. MiniC compiles source to an AST, resolves names, generates real
three-address IR, optionally folds constants, and emits serialized bytecode.
The VM executes that bytecode. It never evaluates the AST directly or delegates
MiniC programs to a host compiler.

MiniC supports checked signed 64-bit `int`, initialized declarations,
assignment, arithmetic, comparisons, unary operators, `print`, lexical blocks,
`if`/`else`, `while`, and comments. Programs need no `main()` function.
It is its own language: no functions, arrays, pointers, strings, floats, input,
`for`, `++`, compound assignment, `&&`, `||`, or preprocessor. It does not
produce native executables. See [the language specification](docs/language.md).

## Build and test

Prerequisites: CMake 3.16 or newer, a C++17 compiler and standard library
(Apple Clang, GCC 9+, or a recent Linux Clang), and a build tool such as Make.
There are no mandatory downloaded compiler/VM/test libraries.

On macOS, install Command Line Tools (`xcode-select --install`) and CMake
(e.g. `brew install cmake`). On Debian/Ubuntu Linux, install `build-essential`
and `cmake` with your package manager. Then, on either platform:

```sh
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

To choose a compiler, use a fresh build directory, e.g.
`cmake -S . -B build-clang -DCMAKE_CXX_COMPILER=clang++`.
The tests include ordinary C++ checks (active even under `NDEBUG`) and CMake
process tests that compare actual exit status, exact stdout, and stderr.
All valid examples have matching `.out` files.

## Compile, inspect, and execute

Run these commands from the project root:

```sh
./build/minic --help
./build/minic --version
./build/minic --tokens examples/factorial.mc
./build/minic --ast examples/factorial.mc
./build/minic --check examples/factorial.mc
./build/minic --ir examples/factorial.mc -O0
./build/minic --ir examples/factorial.mc -O1
./build/minic --emit-bytecode examples/factorial.mc -o build/factorial.mbc
./build/minic-vm build/factorial.mbc
./build/minic-vm build/factorial.mbc --trace --max-steps 100000
./build/minic --run examples/factorial.mc
./build/minic --run examples/factorial.mc -O1
```

Factorial prints `120` followed by a newline. `--run` compiles through IR
and bytecode into the same VM used by `minic-vm`. Normal output goes to stdout;
diagnostics and trace go to stderr. There is no default instruction limit;
use the standalone VM's `--max-steps` for a potentially infinite program.

Exactly one action is required. Unknown/conflicting options, extra input
files, and repeated optimization flags are rejected. `-O0` is the default;
`-O1` performs conservative literal constant folding. `-o` is required and
only allowed for bytecode emission. An output filename starting with `-`
can be written as `./-name.mbc`. `--` ends option parsing for input filenames.
`--help` and `--version` are standalone modes. Frontend dumps show the requested
stage: `--ast` checks syntax; use `--check` to also check names. Optimization
flags affect IR and subsequent stages only.

Exit status is 0 for success, 1 for source errors, 2 for command-line/file I/O/
malformed-bytecode errors, and 3 for VM runtime errors. Compilation and
serialization finish before an output file is opened. A source error cannot
create or overwrite bytecode. The source cannot be its own output file.

## Worked pipeline example

`examples/demo.mc` contains:

```c
print(1 + 2 * 3);
```

`./build/minic --tokens examples/demo.mc` produces:

```text
1:1 print "print"
1:6 ( "("
1:7 INTEGER "1"
1:9 + "+"
1:11 INTEGER "2"
1:13 * "*"
1:15 INTEGER "3"
1:16 ) ")"
1:17 ; ";"
2:1 EOF ""
```

`--ast` produces:

```text
(program
  (print (+ 1 (* 2 3)))
)
```

`--ir` (default `-O0`) produces:

```text
t0 = 2 * 3
t1 = 1 + t0
print t1
halt
```

`--ir -O1` produces `t0 = 6`, `t1 = 7`, `print 7`, `halt`.
To see the complete serialized `-O0` bytecode:

```sh
./build/minic --emit-bytecode examples/demo.mc -o build/demo.mbc
cat build/demo.mbc
```

```text
MINIC 1
SOURCE "examples/demo.mc"
SLOTS 2
CODE 11
PUSH 2 @ 1 13 1 14
PUSH 3 @ 1 13 1 14
MUL @ 1 13 1 14
STORE 0 @ 1 13 1 14
PUSH 1 @ 1 9 1 10
LOAD 0 @ 1 9 1 10
ADD @ 1 9 1 10
STORE 1 @ 1 9 1 10
LOAD 1 @ 1 1 1 18
PRINT @ 1 1 1 18
HALT @ 1 1 1 1
END
```

Slots 0 and 1 store temporaries t0 and t1. Multiplication runs before addition.
`./build/minic-vm build/demo.mbc` prints:

```text
7
```

## Implementation guide

We implement source tracking/diagnostics, a handwritten lexer, a precedence
recursive-descent parser, owned ASTs, semantic scopes/binding, three-address
IR, constant folding, bytecode lowering/serialization/validation/disassembly,
the stack VM, checked arithmetic, CLI drivers, and the test harness.
CMake/CTest only build and test the project; the host C++ compiler compiles
MiniC itself. They are not the MiniC backend.

- [Architecture and ownership](docs/architecture.md)
- [Grammar, scope, and integer semantics](docs/language.md)
- [IR and control-flow lowering](docs/ir.md)
- [Bytecode format and VM contract](docs/bytecode.md)
- [Viva/demo sequence](docs/demo.md)
- [Phase progress and actual verification](docs/progress.md)

The project deliberately uses memory slots for every symbol and temporary.
There is no SSA, register allocation, variable propagation, or dead-code
elimination. Bytecode resource limits and the lack of a default execution
limit are documented in the bytecode guide. Verification on this machine is
macOS/Apple Clang; a Linux run has not been performed here.

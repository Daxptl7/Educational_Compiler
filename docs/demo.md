# Viva and demonstration

From the repository root, first build and run CTest as shown in README.
Then demonstrate these stages (about five minutes):

1. `./build/minic --run examples/factorial.mc` prints 120. Explain that this
   goes through IR and bytecode before VM execution.
2. `./build/minic --tokens examples/demo.mc` shows token kinds, lexemes, and
   source locations. The lexer recognizes words/operators, not statement meaning.
3. `./build/minic --ast examples/demo.mc` shows `(+ 1 (* 2 3))`. Recursive
   descent gives multiplication higher precedence than addition.
4. `./build/minic --ir examples/factorial.mc` shows symbols, temporaries,
   labels, and jumps. Point to the loop condition after its header label and
   the back edge to that label.
5. Compare `./build/minic --ir examples/demo.mc -O0` with the same command
   using `-O1`. Literal arithmetic folds to 7; no mutable variables propagate.
6. `./build/minic --run examples/shadowing.mc` prints 20 then 10. Show distinct
   symbol IDs in its IR. Then run `examples/initializer.mc` to show that an
   inner initializer can read the outer x.
7. `./build/minic --check examples/invalid/undeclared.mc` reports a semantic
   error (status 1); `examples/invalid/missing_semicolon.mc` reports syntax.
8. `./build/minic --run examples/invalid/division_zero.mc` reports a runtime
   error at `/` (status 3). Run `examples/unreachable_error.mc -O1` to show
   that an unreachable failure is harmless and prints 7.
9. `./build/minic --emit-bytecode examples/factorial.mc -o build/factorial.mbc`,
   then `cat build/factorial.mbc`. Run it with
   `./build/minic-vm build/factorial.mbc --trace --max-steps 100000`.
   Program output is stdout; the trace is stderr. The VM reads numeric slots
   and targets, with no parser or source evaluation.
10. `./build/minic-vm tests/bytecode/stack_underflow.mbc` demonstrates a
    controlled runtime fault. `tests/bytecode/bad_target.mbc` is rejected
    during loading. To demonstrate a loop safeguard, run
    `./build/minic-vm tests/bytecode/infinite_loop.mbc --max-steps 20`.

## Questions to explain

| Distinction | Explanation |
| --- | --- |
| Lexer vs parser | Lexer creates tokens from characters; parser organizes tokens according to grammar. |
| Syntax vs semantics | `int x = ;` violates grammar. `print(x);` can parse but fails name resolution without a declaration. |
| AST vs IR | AST preserves nested expression/statement structure. IR flattens computation and control flow into temporaries, labels, and jumps. |
| Compiler vs VM | Compiler resolves source structure/names and emits bytecode. VM runs numeric instructions with a stack and slots. |
| Bytecode vs native code | This bytecode is a custom serialized VM instruction set. It requires minic-vm, rather than running directly on the processor. |
| Why symbol IDs? | Shadowed names must address distinct storage. Names are for diagnostics; IDs define identity. |
| Why check before arithmetic? | C++ signed overflow is undefined behavior. Guards must establish safety before computing results. |
| Why leave invalid constants? | Folding `1/0` into a compile error would break unreachable branches and change runtime behavior. |
| What does CMake do? | Builds MiniC's C++ implementation. It is not the MiniC language compiler. |

The implementation intentionally avoids functions, a type hierarchy, SSA,
register allocation, LLVM, or a GUI so each compilation stage can be explained
and traced in ordinary C++ functions.

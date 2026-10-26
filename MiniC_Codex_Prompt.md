# Codex prompt: Build MiniC, an educational compiler in C++17

You are implementing a complete college Compiler Design project. Create working code in this workspace, not just an explanation or a plan.

Build **MiniC**, a compiler for a deliberately small C-like language, plus its own stack-based virtual machine. The project must be understandable enough for a student to explain in a viva.

## 1. Outcome and working rules

- Implementation language: C++17.
- Build system: CMake, with CTest.
- Platforms: macOS with Apple Clang and Linux with GCC/Clang.
- Use the C++ standard library for the compiler, VM, CLI, and test harness. No mandatory downloaded dependencies.
- Handwrite the lexer and a recursive-descent parser. Use precedence levels for expressions.
- Compile source to three-address intermediate code, then to serialized stack bytecode. Execute the bytecode with your VM.
- Do not replace compilation with direct AST evaluation, Python eval, shell execution of source, or delegation to GCC/Clang.
- Do not add LLVM, a GUI, a web server, or a framework to the required implementation.
- Check existing files and applicable AGENTS.md instructions before editing. Preserve unrelated work.
- Work through the phases below in order. Build and test each phase before continuing. Do not stop after producing a plan or ask for approval between routine phases.
- Keep changes small enough to explain. Prefer clear functions, explicit data types, and ordinary ownership over elaborate design patterns.
- Do not claim a phase is finished while its required behavior is stubbed or its tests are failing.
- If a build tool is unavailable, state the actual blocker and give commands the student can run. Never invent successful test results.
- If execution must pause, leave docs/progress.md with completed work, actual verification, and the next concrete task.

## 2. Language specification

MiniC is not full C and must be documented as its own language.

A program is a sequence of declarations and statements; no main() function is required. Use the .mc extension.

Supported features:
- A single type: int, implemented as a checked signed 64-bit integer.
- Initialized declarations: int name = expression;
- Assignment: name = expression;
- Arithmetic: +, -, *, /, %.
- Unary +, unary -, and logical !.
- Comparisons: <, <=, >, >=, ==, !=.
- Parenthesized expressions.
- print(expression); prints a decimal integer followed by a newline.
- Blocks with lexical scope.
- if (condition) { ... } and optional else { ... }.
- while (condition) { ... }.
- Whitespace, // line comments, and non-nested /* block comments */.

Do not add arrays, pointers, strings, functions, floats, input(), for loops, ++, compound assignment, &&, ||, or a preprocessor.

Semantics:
- Arithmetic operands evaluate left to right.
- Zero means false; any nonzero integer means true.
- Comparisons and ! produce exactly 0 or 1.
- Division truncates toward zero. Remainder follows the corresponding truncating division rule.
- Overflow, division/remainder by zero, and negating the minimum integer are runtime errors with source locations.
- Handle INT64_MIN / -1 and INT64_MIN % -1 explicitly as arithmetic overflow errors. Do not execute undefined C++ arithmetic.
- Decimal literal tokens range from 0 through INT64_MAX. The minus sign is a separate unary operator. Reject oversized literals; document that INT64_MIN can be expressed as (-9223372036854775807 - 1).
- Declarations require an initializer, so no uninitialized variable can be read.
- A declaration becomes visible after its initializer has been resolved. For example, inside a nested scope, int x = x + 1; can refer to an outer x; without an outer x it is an undeclared-name error.
- Reject duplicate declarations in the same scope; allow shadowing in nested scopes.
- Reject use before declaration and assignment to an undeclared variable.
- A name from an inner block is inaccessible after that block.
- A declaration inside a loop executes its initializer on every iteration.
- Conditions and assignments use int only; assignment is a statement, not an expression.

Use this grammar as the contract:

program        = { item } EOF ;
item           = declaration | statement ;
declaration    = "int" IDENT "=" expression ";" ;
statement      = assignment | printStmt | ifStmt | whileStmt | block ;
assignment     = IDENT "=" expression ";" ;
printStmt      = "print" "(" expression ")" ";" ;
ifStmt         = "if" "(" expression ")" block [ "else" block ] ;
whileStmt      = "while" "(" expression ")" block ;
block          = "{" { item } "}" ;
expression     = equality ;
equality       = comparison { ("==" | "!=") comparison } ;
comparison     = term { ("<" | "<=" | ">" | ">=") term } ;
term           = factor { ("+" | "-") factor } ;
factor         = unary { ("*" | "/" | "%") unary } ;
unary          = ("+" | "-" | "!") unary | primary ;
primary        = INTEGER | IDENT | "(" expression ")" ;

Binary operators within each level associate left to right. Unary operators associate right to left. Identifiers use ASCII letters or underscore first, followed by ASCII letters, digits, or underscores. Keywords cannot be identifiers. Require braces for if, else, and while bodies; else-if is not a separate feature.

## 3. Architecture and boundaries

Pipeline:
Source -> Lexer -> Tokens -> Parser -> AST -> Semantic Analyzer -> Bound AST -> Three-Address IR -> Optional IR Optimization -> Bytecode Generator -> Bytecode File -> VM.

Design independent components:
- SourceManager and DiagnosticEngine: source text, file names, line/column mapping, messages.
- Lexer: tokens with kinds, lexemes, and source spans.
- Parser: AST construction and syntax diagnostics.
- AST: expression and statement node types with source spans.
- SemanticAnalyzer: scopes, declarations, stable symbol IDs, and resolved references.
- IRGenerator: lower the resolved AST into explicit instructions and labels.
- IROptimizer: semantics-preserving constant folding.
- BytecodeGenerator: allocate slots and translate IR instructions.
- BytecodeReader/Writer: versioned serialization, validation, and disassembly.
- VirtualMachine: instruction dispatch, stack, slots, runtime diagnostics.
- CLI drivers: orchestrate components without embedding compiler logic in main().

Use std::unique_ptr for owning AST edges and enum class for finite instruction/token kinds. Keep symbol IDs distinct from names so shadowed variables cannot alias accidentally.

Suggested organization; adjust file names if that improves clarity:

CMakeLists.txt
README.md
include/minic/        Public component headers
src/                 Component implementations
tools/               minic and minic-vm entry points
tests/               C++ unit and integration tests
examples/            Valid .mc programs and expected outputs
examples/invalid/    Programs demonstrating diagnostics
docs/language.md
docs/architecture.md
docs/ir.md
docs/bytecode.md
docs/demo.md
docs/progress.md

## 4. Intermediate representation

Implement an explicit three-address IR, not just printed pseudocode.

Use tagged operands for integer constants, resolved symbols, and temporary values. Include instructions for:
- Copy.
- Unary operation.
- Binary operation.
- Label.
- Unconditional jump.
- Conditional jump when an operand is zero.
- Print.
- Halt.

Instructions carry source spans. Resolve symbol references before IR generation.

Lower if/else and while into labels and jumps. Emit a while condition inside the loop header so it is recomputed on every iteration.

Start with straightforward memory-based temporaries; SSA, phi nodes, and register allocation are unnecessary.

For a + b * 2, the IR should visibly compute multiplication before addition.

## 5. Bytecode and VM contract

Required instruction families:
- PUSH integer
- LOAD slot
- STORE slot
- ADD, SUB, MUL, DIV, MOD
- NEG, NOT
- EQ, NE, LT, LE, GT, GE
- JMP instruction_index
- JZ instruction_index
- PRINT
- HALT

Use zero-based numeric slots and instruction indexes in executable bytecode; human-readable names belong in debug information or dumps.

A simple backend may allocate a slot for every symbol and temporary. Compute a value on the operand stack and STORE it when needed; advanced allocation is out of scope.

Document stack effects:
- Binary operations pop right operand, then left operand, and push left OP right.
- Unary operations pop one value and push one result.
- STORE, JZ, and PRINT consume one value.
- LOAD and PUSH push one value.
- JMP and HALT do not change the stack.

Use a simple versioned textual bytecode format with a magic header, format version, slot count, instructions, and source-location metadata. Define the format precisely and implement a strict reader and writer. This is serialized VM code, not native machine code.

Validate headers, instruction names, operand counts/types, slot bounds, and jump targets when loading. Detect stack underflow, uninitialized slot reads, arithmetic errors, and invalid execution state in the VM. Malformed input must produce controlled errors rather than crashes.

Provide an optional instruction limit for accidental infinite loops and a trace mode. Keep trace and diagnostic output on stderr. Normal program output goes to stdout.

## 6. CLI contract

Build two executables, minic and minic-vm.

Support:
- minic --help
- minic --version
- minic --tokens file.mc
- minic --ast file.mc
- minic --check file.mc
- minic --ir file.mc -O0
- minic --ir file.mc -O1
- minic --emit-bytecode file.mc -o file.mbc
- minic --run file.mc
- minic --run file.mc -O1
- minic-vm file.mbc
- minic-vm file.mbc --trace
- minic-vm file.mbc --max-steps 100000

Require an explicit action mode; explain usage when it is missing or conflicting. Reject unknown options. Use -O0 by default.

--run must compile to an in-memory bytecode program and execute the same VM used by minic-vm. It must not bypass IR or bytecode generation.

Exit codes:
- 0: success.
- 1: source compilation error.
- 2: CLI, file I/O, or malformed bytecode error.
- 3: VM runtime error.

For --run, stdout contains only the source program's output. Dumps are deterministic. Do not create or overwrite an output file until compilation succeeds.

## 7. Phase-by-phase implementation

### Phase 0: Project skeleton and specification
Create CMake targets, a minimal CTest harness, CLI skeletons, and the language/architecture documents. Use C++17 without compiler extensions. Enable useful warnings for supported compilers.

Acceptance: a clean configure/build succeeds, help works, and a smoke test runs through CTest.

### Phase 1: Lexer
Implement source tracking, tokens, maximal-munch operators, identifiers, integers, whitespace, and comments. Report invalid characters, oversized integers, and unterminated block comments.

Acceptance: token tests cover every token kind, adjacent operators, keyword boundaries, comments, EOF, and line/column positions.

### Phase 2: Parser and AST
Implement the grammar exactly, with readable deterministic AST dumps. Represent declarations, assignment, print, blocks, if/else, while, literals, variable references, unary expressions, and binary expressions.

Use panic-mode recovery at sensible statement boundaries, such as semicolons and closing braces. Guarantee recovery makes progress and does not loop forever.

Acceptance: test precedence, associativity, nested blocks, nested control flow, missing semicolons, malformed expressions, unmatched braces, and EOF. Invalid ASTs must not proceed to code generation.

### Phase 3: Semantic analysis
Implement nested scopes, stable symbol IDs, name resolution, and all declaration visibility rules.

Acceptance: test undeclared reads/writes, duplicates, valid shadowing, initializer lookup, scope exit, keyword misuse, and loop-local declarations.

### Phase 4: IR
Generate real IR for expressions, assignment, print, blocks, if/else, and loops. Print deterministic temporary and label names. Stop this stage if earlier compilation errors exist.

Acceptance: structural IR tests verify evaluation order, precedence, both branches, back edges, condition reevaluation, and distinct symbols for shadowed names.

### Phase 5: Bytecode generation and serialization
Lower IR to stack instructions, resolve labels, allocate slots, preserve source mapping, and implement bytecode round trips and disassembly.

Acceptance: test jump fixups, arithmetic operand order, slot separation, round-trip equivalence, unsupported versions, invalid operands, and invalid targets.

### Phase 6: Virtual machine and complete execution
Implement the dispatch loop and checked arithmetic. Share arithmetic helpers with the optimizer where appropriate to avoid inconsistent semantics.

Acceptance: compile and execute examples for arithmetic, if/else, while, factorial, shadowing, and nested loops. Verify runtime errors and instruction limits.

### Phase 7: Optional optimization through -O1
Implement conservative constant folding only when an IR operation's operands are known integer literals and evaluation is valid.

Do not fold operations that overflow or divide/remainder by zero. Leave them for runtime so unreachable branches remain harmless and error behavior matches -O0. Do not propagate mutable variables across branches/loops or add dead-code elimination without a sound analysis.

Acceptance: compare exact output, exit code, and runtime error location between -O0 and -O1 for valid programs, reachable arithmetic failures, and unreachable failing expressions. Include negative integer arithmetic.

### Phase 8: End-to-end polish and documentation
Finish all CLI modes, diagnostics, examples, tests, and project documentation. Remove misleading stubs and dead scaffolding.

Acceptance: a fresh build succeeds and ctest --test-dir build --output-on-failure passes. Report actual command results.

## 8. Required examples and tests

Provide examples with exact expected output:

Arithmetic:
int a = 10;
int b = 20;
print(a + b * 2);
Expected output: 50 followed by newline.

Factorial:
int n = 5;
int result = 1;
while (n > 1) {
    result = result * n;
    n = n - 1;
}
print(result);
Expected output: 120 followed by newline.

Shadowing:
int x = 10;
{
    int x = 20;
    print(x);
}
print(x);
Expected output: 20 then 10, on separate lines.

Also include:
- if/else with both true and false conditions.
- Nested loops.
- A zero-iteration loop.
- A loop-local variable initialized afresh each iteration.
- Negative division and remainder.
- int x = 0; if (x) { print(1 / 0); } print(7); which prints only 7 without a runtime error at either optimization level.
- Undeclared variables, duplicate declarations, missing punctuation, bad comments, overflow, and division by zero.
- Malformed-bytecode cases and stack underflow.

Use CTest to run tests automatically. Avoid a test harness whose checks disappear under NDEBUG. Compare output and exit status, not just whether execution completes.

## 9. Documentation and submission deliverables

README.md must explain:
- What MiniC supports and its limits.
- Prerequisites and macOS/Linux build commands.
- How to compile, inspect, and execute programs.
- How to run the tests.
- A worked example showing source, tokens, AST, IR, bytecode, and result.
- Which components we implemented and which tools only build the project.

Other documents:
- language.md: grammar, precedence, scope, integer semantics.
- architecture.md: a compact Mermaid pipeline and component responsibilities.
- ir.md: operand/instruction definitions and control-flow lowering.
- bytecode.md: file format, opcodes, operand order, stack effects, validation.
- demo.md: a short viva/demo sequence and explanations of lexer vs parser, syntax vs semantics, AST vs IR, compiler vs VM, and bytecode vs native code.
- progress.md: phase status, verified commands, known limitations.

Expected build commands:
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure

After completing implementation, summarize what works, the real build/test results, how to run the factorial example, and any remaining limitations. Describe this accurately as a compiler targeting a custom stack VM; do not claim full C compatibility or native executable generation.

Start by inspecting the workspace, then implement Phase 0 and continue through all phases.


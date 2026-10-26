# Architecture blueprint

Source -> Lexer -> Tokens -> Parser -> AST -> Semantic Analyzer -> Bound AST
-> Three-address IR -> Optional Optimizer -> Bytecode Generator -> Bytecode
-> Writer/Reader -> Stack Virtual Machine.

Phase 0 provides the build, specification, smoke test, and help skeletons.
Subsequent commits implement each component in order. No source execution
is implemented by this initial scaffold. CMake builds MiniC; it is not the
MiniC compiler backend.

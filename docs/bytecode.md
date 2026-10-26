# Bytecode and VM

MiniC emits serialized instructions for its own VM, not machine code. Slots
and instruction indexes are zero-based integers. Source names and locations
are debug information; executable operands never contain variable names.

## Version 1 format

A file has these physical lines in order (there are no comments/blank lines):

```text
MINIC 1
SOURCE "source.mc"
SLOTS 2
CODE 3
PUSH 42 @ 1 7 1 9
PRINT @ 1 1 1 11
HALT @ 1 1 1 1
END
```

`MINIC 1` is the exact magic/version line. `SOURCE` is followed by one
mandatory quoted filename, using C++ `std::quoted` escaping: `\"` encodes a
quote and `\\` encodes a backslash. Filenames cannot contain ASCII control
characters and are limited to 1800 bytes. Empty quoted names are permitted
for bytecode without a known source. The VM never opens the source file.

`SLOTS N` declares storage, and `CODE N` declares the exact instruction count.
Counts use unsigned decimal digits. Each instruction line is:

```text
OPCODE [signed_decimal_operand] @ start_line start_column end_line end_column
```

The operand exists exactly for PUSH, LOAD, STORE, JMP, and JZ. PUSH accepts the
full signed 64-bit range, including INT64_MIN (unlike a source literal token).
Numeric operands reject `+`, non-decimal text, partial numbers, and overflow.
Slot/jump operands must be nonnegative and in bounds. Locations are unsigned,
positive 1-based byte coordinates with end >= start, describing a half-open
span. A zero-width location, such as the generated HALT at 1:1, is permitted.
Metadata is required even on HALT. Tokens within count/instruction lines are
whitespace separated. The writer emits single spaces and LF; the reader also
accepts CRLF and a final END without a terminating newline. Headers and END
have their exact spelling. Extra records, even blank lines after END, fail.

Load-time validation rejects bad headers/versions, missing records, unknown
opcodes, incorrect operand counts/types, invalid coordinates, out-of-range
slots/targets, and trailing data. Up to 1,000,000 slots and 1,000,000
instructions are allowed; code must be nonempty. Physical lines are limited
to 4096 bytes (excluding LF, including an optional CR). The VM operand stack
also has a 1,000,000-value limit. These are resource safeguards, not language
type rules. The writer validates the same invariants before serializing.

## Instructions and stack effects

Here `a` is the left operand and `b` the right operand. The right operand is
popped first, then the left. In particular SUB computes `a - b`, DIV computes
`a / b`, and MOD computes `a % b`.

| Instruction | Operand | Stack before → after | Meaning |
| --- | --- | --- | --- |
| PUSH | signed integer | `... → ... value` | Push constant |
| LOAD | slot | `... → ... value` | Read initialized slot |
| STORE | slot | `... value → ...` | Write slot |
| ADD, SUB, MUL, DIV, MOD | none | `... a b → ... result` | Checked arithmetic |
| NEG, NOT | none | `... a → ... result` | Checked negation; zero test |
| EQ, NE, LT, LE, GT, GE | none | `... a b → ... result` | Comparison, returning 0 or 1 |
| JMP | instruction index | unchanged | Set instruction pointer |
| JZ | instruction index | `... value → ...` | Jump if consumed value is zero |
| PRINT | none | `... value → ...` | Print decimal value and newline |
| HALT | none | unchanged | Finish; requires empty stack |

Unary + needs no arithmetic opcode: its input is copied. All slots begin
uninitialized; LOAD checks initialization. STORE, PRINT, and JZ consume one
value; binary and unary operations check stack depth. Arithmetic checks
match [language.md](language.md), including overflow in both minimum/-1
cases. A branch may reach a structurally valid instruction with an invalid
stack; this is detected during execution, not with a static type verifier.
A missing HALT can load structurally, but falling past the code is a runtime
error. A nonempty stack at HALT is an invalid execution state.

## Lowering, execution, and inspection

The backend assigns slots `[0,symbolCount)` to symbols and subsequent slots
to temporaries. For `t0 = s0 - 3`, it emits `LOAD 0`, `PUSH 3`, `SUB`,
`STORE <t0 slot>`. Labels have no bytecode instruction. The backend records
the instruction index at each IR label and patches every JMP/JZ afterward.
A label at the end of a block targets the next emitted instruction, normally
another statement or HALT.

`BytecodeWriter::write`, `BytecodeReader::read`, and `disassemble` are public
library functions. Disassembly prints `index: OPCODE [operand] @ line:column`.
The file itself is also readable with `cat`; no disassembly CLI mode is
required. `minic-vm --trace` prints index, operation, operand, stack depth,
and source location to stderr before each executed instruction.

`--max-steps N` permits exactly N dispatches, counting HALT and jumps.
Attempting another instruction reports a runtime error at that instruction.
N must be a positive decimal uint64. Without this option, execution has no
step limit. Output remains on stdout; runtime/trace text remains on stderr.
Malformed bytecode returns exit 2; runtime faults return exit 3.

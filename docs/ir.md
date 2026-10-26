# Three-address IR

IR is a vector of `IRInstruction` values, not a text-only display. Every
instruction has a half-open source span; arithmetic instructions use the
operator's span. `dumpIR` is a deterministic rendering of these instructions.

## Operands and instructions

Operands are tagged as constant `int64`, resolved symbol `sN`, or temporary
`tN`. Symbol IDs are distinct from temporary IDs; names appear only in symbol
comments. All IDs start at zero. Every generated temporary has one defining
instruction, although that instruction may execute many times inside a loop.

| Kind | Dump | Meaning |
| --- | --- | --- |
| Copy | `s0 = t0` | Copy value to symbol or temporary |
| Unary | `t0 = - s0` | Apply unary +, -, or ! |
| Binary | `t1 = s0 + t0` | Apply arithmetic or comparison |
| Label | `L0:` | Mark a control-flow destination |
| Jump | `jump L0` | Unconditional branch |
| JumpZero | `jump_zero t0, L1` | Branch when operand is zero |
| Print | `print t0` | Print integer and newline |
| Halt | `halt` | Finish program |

Declarations and assignments copy into their symbol slot. Blocks require no
runtime instruction beyond their contents: name resolution already separates
shadowed storage. Expressions lower left operand first, then right operand,
then their operator. `a + b * 2` becomes:

```text
t0 = s1 * 2
t1 = s0 + t0
```

There are no expression side effects in this language. No AST evaluation is
used during execution.

## Control flow

For `if (x) { print(1); } else { print(2); }`:

```text
jump_zero s0, L0
print 1
jump L1
L0:
print 2
L1:
halt
```

An if without else needs one exit label. For
`while (n > 1) { n = n - 1; }`:

```text
L0:
t0 = s0 > 1
jump_zero t0, L1
t1 = s0 - 1
s0 = t1
jump L0
L1:
halt
```

The back edge targets the label *before* condition evaluation. A declaration
in the body has its copy/initializer instructions inside this region, so it
initializes afresh each iteration. There are no phi nodes or SSA constraints.

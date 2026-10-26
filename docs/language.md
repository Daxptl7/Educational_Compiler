# MiniC language

MiniC is a small C-like language with checked signed 64-bit integers,
initialized declarations, assignment, print, blocks, if/else, and while.
Programs are sequences of declarations and statements, without main().
Braces are required around control-flow bodies. Assignment is a statement.
Lexical scopes allow shadowing, but not duplicate declarations in one scope.
An initializer resolves before its declaration becomes visible.

## Grammar

```ebnf
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
```

Binary precedence from lowest to highest is equality, comparisons, addition/
subtraction, multiplication/division/remainder. Binary operators associate
left to right; unary operators associate right to left. Operands evaluate
left to right. Zero is false; all other integers are true. Comparisons and
logical NOT return exactly 0 or 1. Print appends a newline.

Identifiers start with an ASCII letter or underscore and continue with ASCII
letters, digits, or underscores. `int`, `print`, `if`, `else`, and `while`
are reserved. Whitespace, `//` comments, and non-nested `/* ... */` comments
are ignored. No empty statements, else-if, or assignment expressions exist.

## Integer and scope rules

Literals contain decimal digits only, from 0 through 9223372036854775807.
The minus sign is a separate unary operator. Write the minimum integer as
`(-9223372036854775807 - 1)`. Integers use signed 64-bit storage. Addition,
subtraction, multiplication, and negation check overflow. Division truncates
toward zero; remainder satisfies `a = (a / b) * b + a % b`. Division and
remainder by zero are runtime errors. Both `INT64_MIN / -1` and
`INT64_MIN % -1` are overflow errors. Runtime diagnostics point to the
operator in the original source, including after optimization.

Every declaration requires an initializer. A name becomes visible only
after that initializer is resolved: `{ int x = x + 1; }` reads an outer x
if one exists, otherwise it is an error. Use before declaration, undeclared
assignment, and same-scope duplicates are errors. Nested blocks can shadow
outer names. A block's names disappear at its closing brace. A declaration
in a loop runs its initializer on every iteration.

MiniC has no functions, main(), arrays, pointers, strings, floats, input,
for loops, increment, compound assignment, logical AND/OR, or preprocessor.
It targets custom bytecode, not native executables or full C compatibility.

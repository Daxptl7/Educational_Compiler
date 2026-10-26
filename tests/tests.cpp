#include "minic/lexer.hpp"
#include "minic/parser.hpp"
#include "minic/semantic.hpp"
#include "minic/ir.hpp"
#include "minic/bytecode.hpp"
#include "minic/arithmetic.hpp"
#include "minic/vm.hpp"
#include <iostream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>
using namespace minic;
namespace {
int checks = 0;
void require(bool condition, const std::string& message) {
    ++checks;
    if (!condition) throw std::runtime_error(message);
}
void lexerTests() {
    SourceManager s("test.mc", "name 0 int print if else while + - * / % ! = == != < <= > >= ( ) { } ;");
    DiagnosticEngine d;
    auto t = Lexer(s, d).scan();
    require(!d.hasErrors(), "all tokens accepted");
    require(t.size() == 26, "every token plus EOF");
    for (std::size_t i = 0; i < 25; ++i)
        require(static_cast<std::size_t>(t[i].kind) == i + 1, "token kind order");
    require(t.back().kind == TokenKind::End, "EOF token");
    SourceManager adjacent("test.mc", "===!==<<=>>=!! intx print_ _a9 // ok\n/* a\nb */ 42");
    DiagnosticEngine ad;
    auto a = Lexer(adjacent, ad).scan();
    const std::vector<TokenKind> expected{TokenKind::EqualEqual, TokenKind::Equal,
        TokenKind::BangEqual, TokenKind::Equal, TokenKind::Less, TokenKind::LessEqual,
        TokenKind::Greater, TokenKind::GreaterEqual, TokenKind::Bang, TokenKind::Bang,
        TokenKind::Identifier, TokenKind::Identifier, TokenKind::Identifier, TokenKind::Integer, TokenKind::End};
    require(!ad.hasErrors() && a.size() == expected.size(), "maximal munch and comments");
    for (std::size_t i = 0; i < a.size(); ++i) require(a[i].kind == expected[i], "adjacent token");
    auto loc = adjacent.location(a[13].span.begin);
    require(loc.line == 3 && loc.column == 6, "line and column after comments");
    for (const auto& text : {"9223372036854775808", "@", "/*", "/* no end*", "&|"}) {
        SourceManager bad("bad.mc", text); DiagnosticEngine bd;
        auto tokens = Lexer(bad, bd).scan();
        require(bd.hasErrors() && tokens.back().kind == TokenKind::End, "lexical error controlled");
    }
    SourceManager max("max.mc", "9223372036854775807"); DiagnosticEngine md;
    require(Lexer(max, md).scan()[0].integer == std::numeric_limits<std::int64_t>::max()
            && !md.hasErrors(), "maximum literal");
    SourceManager empty("empty.mc", ""); DiagnosticEngine ed;
    require(Lexer(empty, ed).scan().size() == 1, "empty input");
    std::ostringstream out; dumpTokens(t, s, out);
    require(out.str().find("1:1 IDENT \"name\"") == 0, "deterministic token dump");
}
void parserTests() {
    SourceManager s("ast.mc", "int a = 1 + 2 * 3; print(10 - 3 - 2); print(!!-a); print((1 + 2) * 3); print(1 < 2 == 1); { if (a) { while (a) { a = a - 1; } } else { print(0); } }");
    DiagnosticEngine d; auto t = Lexer(s, d).scan(); auto p = Parser(t, d).parse();
    require(!d.hasErrors() && p.items.size() == 6, "nested grammar");
    std::ostringstream out; dumpAST(p, out);
    for (const auto& fragment : {"(+ 1 (* 2 3))", "(- (- 10 3) 2)", "(! (! (- a)))", "(* (+ 1 2) 3)", "(== (< 1 2) 1)", "(while a", "(else"})
        require(out.str().find(fragment) != std::string::npos, "AST structure: " + std::string(fragment));
    require(p.items[0]->expression->operatorSpan.begin == 10, "operator span retained");
    for (const auto& text : {"int a = 1", "print(1 + ); print(2);", "{ int x = 2;", "}", "if (1) print(2);", "if (1) {} else if (0) {}", "while (1) {", "int = ; ; print(; }", "print((1);", "x = (x = 2);", "", "print(+);"}) {
        SourceManager bad("parse.mc", text); DiagnosticEngine bd;
        auto bt = Lexer(bad, bd).scan(); auto bp = Parser(bt, bd).parse();
        require(std::string(text).empty() ? !bd.hasErrors() : bd.hasErrors(), "parser recovery/EOF: " + std::string(text));
    }
    SourceManager recovery("recover.mc", "int a = 1 int b = 2; print(3 + ); print(4);"); DiagnosticEngine rd;
    auto rt = Lexer(recovery, rd).scan(); auto rp = Parser(rt, rd).parse();
    require(rd.errors().size() == 2 && rp.items.size() == 2, "panic recovery preserves subsequent statements");
}
struct Frontend {
    SourceManager source;
    DiagnosticEngine diagnostics;
    std::vector<Token> tokens;
    Program ast;
    std::optional<BoundProgram> bound;
    explicit Frontend(const std::string& text) : source("test.mc", text) {
        tokens = Lexer(source, diagnostics).scan();
        if (!diagnostics.hasErrors()) ast = Parser(tokens, diagnostics).parse();
        auto result = SemanticAnalyzer(diagnostics).analyze(ast);
        if (result) bound.emplace(std::move(*result));
    }
};
void semanticTests() {
    Frontend good("int x = 10; { int x = x + 1; print(x); } print(x); while (x) { int y = x; x = x - 1; }");
    require(good.bound.has_value() && good.bound->symbols.size() == 3, "valid shadowing and loop local");
    const auto& outer = *good.ast.items[0]; const auto& inner = *good.ast.items[1]->items[0];
    require(outer.symbol != inner.symbol && inner.expression->left->symbol == outer.symbol, "initializer sees outer symbol");
    require(good.ast.items[1]->items[1]->expression->symbol == inner.symbol, "nested reference bound");
    require(good.ast.items[2]->expression->symbol == outer.symbol, "outer restored");
    for (const auto& text : {"print(x);", "x = 2;", "int x = 1; int x = 2;", "int x = x + 1;", "{int x = 1;} print(x);", "print(x); int x = 1;", "while (0) {int y = 1;} print(y);", "int while = 2;", "int x;"}) {
        Frontend bad(text);
        require(!bad.bound && bad.diagnostics.hasErrors(), "invalid names/syntax blocked: " + std::string(text));
    }
    Frontend branches("if (1) { int x = 1; } else { int x = 2; } int x = 3;");
    require(branches.bound && branches.bound->symbols.size() == 3, "branch scopes independent");
}
void irTests() {
    Frontend f("int a = 10; int b = 20; print(a + b * 2); print((1 + 2) * (3 + 4));");
    auto ir = IRGenerator().generate(*f.bound); std::ostringstream out; dumpIR(ir, out);
    require(out.str().find("t0 = s1 * 2\nt1 = s0 + t0\nprint t1") != std::string::npos, "IR precedence");
    require(out.str().find("t2 = 1 + 2\nt3 = 3 + 4\nt4 = t2 * t3") != std::string::npos, "IR evaluation order");
    Frontend flow("int x = 2; while (x > 0) { int y = x + 1; if (y) {print(y);} else {print(x);} x = x - 1; } {int x = 8; print(x);} print(x);");
    auto control = IRGenerator().generate(*flow.bound); std::ostringstream co; dumpIR(control, co);
    require(co.str().find("L0:\nt0 = s0 > 0\njump_zero t0, L1") != std::string::npos, "loop condition inside header");
    require(co.str().find("jump L0\nL1:") != std::string::npos, "loop back edge");
    require(co.str().find("jump_zero s1, L2\nprint s1\njump L3\nL2:\nprint s0\nL3:") != std::string::npos, "both if branches");
    require(co.str().find("s2 = 8\nprint s2\nprint s0") != std::string::npos, "shadow symbols distinct in IR");
    require(control.instructions.back().kind == IRKind::Halt && control.labelCount == 4, "IR halt and labels");
}
void bytecodeTests() {
    Frontend f("int x = 2; {int x = 3; print(x - 1);} while (x > 0) {x = x - 1;} print(x);");
    const auto ir = IRGenerator().generate(*f.bound);
    auto p = BytecodeGenerator().generate(ir, f.source);
    require(p.slotCount == ir.symbols.size() + ir.temporaryCount, "symbol/temp slot separation");
    bool back = false, forward = false;
    for (std::size_t pc = 0; pc < p.instructions.size(); ++pc) {
        const auto& i = p.instructions[pc];
        if (i.op == OpCode::Jmp) back = static_cast<std::size_t>(i.argument) < pc;
        if (i.op == OpCode::Jz) forward = static_cast<std::size_t>(i.argument) > pc;
    }
    require(back && forward, "numeric jump fixups");
    require(p.instructions[4].op == OpCode::Load && p.instructions[4].argument == 1 && p.instructions[5].op == OpCode::Push && p.instructions[5].argument == 1 && p.instructions[6].op == OpCode::Sub, "left/right bytecode operand order");
    std::ostringstream serialized; BytecodeWriter::write(p, serialized);
    std::istringstream in(serialized.str()); auto roundtrip = BytecodeReader::read(in);
    std::ostringstream again; BytecodeWriter::write(roundtrip, again);
    require(again.str() == serialized.str(), "bytecode exact round trip");
    std::ostringstream dis; disassemble(roundtrip, dis);
    require(dis.str().find("0: PUSH 2 @ 1:1") == 0, "indexed disassembly");
    const std::string prefix = "MINIC 1\nSOURCE \"bad.mc\"\nSLOTS 1\nCODE 1\n";
    for (const auto& text : {std::string("MINIC 2\n"), std::string("garbage"), prefix + "BOGUS @ 1 1 1 1\nEND\n", prefix + "LOAD 1 @ 1 1 1 1\nEND\n", prefix + "JMP -1 @ 1 1 1 1\nEND\n", prefix + "JZ 1 @ 1 1 1 1\nEND\n", prefix + "PUSH x @ 1 1 1 1\nEND\n", prefix + "PUSH 9223372036854775808 @ 1 1 1 1\nEND\n", prefix + "ADD 1 @ 1 1 1 1\nEND\n", prefix + "HALT @ 0 1 1 1\nEND\n", prefix + "HALT @ 1 2 1 1\nEND\n", prefix + "HALT @ 1 1 1 1\nEND\nextra", prefix + "PUSH\nEND\n", prefix + "HALT @ 1 1 1 1\n", std::string("MINIC 1\nSOURCE unquoted\nSLOTS 0\nCODE 1\nHALT @ 1 1 1 1\nEND\n")}) {
        bool failed = false; try { std::istringstream bad(text); BytecodeReader::read(bad); } catch (const BytecodeError&) { failed = true; }
        require(failed, "strict malformed bytecode rejection");
    }
    const std::vector<std::string> malformed{
        "MINIC 1\nSOURCE \"x\"\nSLOTS -1\nCODE 0\nEND\n",
        "MINIC 1\nSOURCE \"x\"\nSLOTS 1000001\nCODE 0\nEND\n",
        "MINIC 1\nSOURCE \"x\"\nSLOTS 0\nCODE 1000001\nEND\n",
        "MINIC 1\nSOURCE \"x\"\nSLOTS 0\nCODE 0\nEND\n",
        "MINIC 1\nSOURCE \"unterminated\n",
        prefix + "LOAD -1 @ 1 1 1 1\nEND\n",
        prefix + "PUSH -9223372036854775809 @ 1 1 1 1\nEND\n",
        prefix + "HALT @ -1 1 1 1\nEND\n",
        prefix + "PUSH 1x @ 1 1 1 1\nEND\n",
        prefix + "PUSH +1 @ 1 1 1 1\nEND\n",
        prefix + "HALT @ 184467440737095516160 1 1 1\nEND\n",
        prefix + "HALT @ 1 1 1 1\nEND\n\n",
        std::string(4097, 'A')
    };
    for (const auto& text : malformed) {
        bool failed = false;
        try { std::istringstream bad(text); BytecodeReader::read(bad); }
        catch (const BytecodeError&) { failed = true; }
        require(failed, "bytecode numeric/resource/framing rejection");
    }
    BytecodeProgram edge; edge.sourceFile = "folder/quoted\"name\\file.mc";
    edge.instructions = {{OpCode::Push, std::numeric_limits<std::int64_t>::min(), {2, 3}, {2, 4}},
                         {OpCode::Print, 0, {2, 1}, {2, 6}}, {OpCode::Halt, 0, {2, 6}, {2, 6}}};
    std::ostringstream edgeOut; BytecodeWriter::write(edge, edgeOut);
    std::string crlf;
    for (char c : edgeOut.str()) { if (c == '\n') crlf += '\r'; crlf += c; }
    std::istringstream edgeIn(crlf); const auto edgeBack = BytecodeReader::read(edgeIn);
    require(edgeBack.sourceFile == edge.sourceFile && edgeBack.instructions[0].argument == std::numeric_limits<std::int64_t>::min(), "quoted filename, CRLF, and signed bytecode minimum");
    auto noNewline = edgeOut.str(); noNewline.pop_back(); std::istringstream noNewlineIn(noNewline);
    require(BytecodeReader::read(noNewlineIn).instructions.size() == 3, "END without final newline");
    auto invalid = edge; invalid.instructions[0].op = static_cast<OpCode>(100);
    bool rejected = false;
    try { validateBytecode(invalid); } catch (const BytecodeError&) { rejected = true; }
    require(rejected, "invalid in-memory opcode rejected");
}
struct Execution { int status = 0; std::string output, error, trace; };
Execution execute(const std::string& text, bool serialized = false) {
    Frontend f(text); require(f.bound.has_value(), "execution source compiles: " + text);
    auto ir = IRGenerator().generate(*f.bound);
    auto p = BytecodeGenerator().generate(ir, f.source);
    if (serialized) { std::ostringstream out; BytecodeWriter::write(p, out); std::istringstream in(out.str()); p = BytecodeReader::read(in); }
    std::ostringstream output, trace; Execution result;
    try { VirtualMachine().execute(p, output, trace, {true, 100000}); }
    catch (const RuntimeError& e) { result.status = 3; result.error = std::to_string(e.location().line) + ':' + std::to_string(e.location().column) + ": " + e.what(); }
    result.output = output.str(); result.trace = trace.str(); return result;
}
void vmTests() {
    const std::vector<std::pair<std::string, std::string>> programs{
        {"int a=10; int b=20; print(a+b*2);", "50\n"},
        {"int n=5; int result=1; while(n>1){result=result*n; n=n-1;} print(result);", "120\n"},
        {"int x=10; {int x=20; print(x);} print(x);", "20\n10\n"},
        {"if(7){print(1);}else{print(2);} if(0){print(3);}else{print(4);}", "1\n4\n"},
        {"int i=0; int count=0; while(i<3){int j=0; while(j<2){count=count+1; j=j+1;} i=i+1;} print(count);", "6\n"},
        {"int i=0; while(i<3){int x=0; x=x+1; print(x); i=i+1;} while(0){print(99);}", "1\n1\n1\n"},
        {"print(-7/3); print(-7%3); print(7/-3); print(7%-3);", "-2\n-1\n-2\n1\n"},
        {"print(1==1); print(1!=1); print(2<1); print(2<=2); print(3>2); print(3>=4); print(!0); print(!-2); print(+5);", "1\n0\n0\n1\n1\n0\n1\n0\n5\n"},
        {"int x=0; if(x){print(1/0);} print(7);", "7\n"},
        {"int x=10; {int x=x+1; print(x);} print(x);", "11\n10\n"},
        {"print(-9223372036854775807-1);", "-9223372036854775808\n"},
        {"", ""}
    };
    for (const auto& test : programs) for (bool serialized : {false, true}) {
        const auto r = execute(test.first, serialized);
        require(r.status == 0 && r.output == test.second, "VM program output: " + test.first);
        require(r.trace.find("HALT") != std::string::npos, "trace separate from program output");
    }
    for (const auto& text : {"print(9223372036854775807+1);", "print((-9223372036854775807-1)-1);", "print(9223372036854775807*2);", "print(-(-9223372036854775807-1));", "print((-9223372036854775807-1)/-1);", "print((-9223372036854775807-1)%-1);", "print(1/0);", "print(1%0);"}) {
        const auto r = execute(text);
        require(r.status == 3 && !r.error.empty(), "checked arithmetic runtime error");
    }
    require(execute("\nprint(1 / 0);").error == "2:9: division by zero", "operator runtime source location");
    const auto order = execute("print((1 / 0) + (9223372036854775807 + 1));");
    require(order.error.find("division by zero") != std::string::npos, "left error occurs first");
    require(execute("while(1){}").error.find("instruction limit") != std::string::npos, "instruction limit");
    for (const auto& op : {"ADD", "NEG", "STORE 0", "JZ 0", "PRINT", "LOAD 0", "PUSH 1"}) {
        std::istringstream in(std::string("MINIC 1\nSOURCE \"vm.mc\"\nSLOTS 1\nCODE 1\n") + op + " @ 2 3 2 4\nEND\n");
        auto p = BytecodeReader::read(in); std::ostringstream out, trace; bool failed = false;
        try { VirtualMachine().execute(p, out, trace); } catch (const RuntimeError& e) { failed = e.location().line == 2; }
        require(failed, "runtime malformed stack/state handled: " + std::string(op));
    }
    const auto low = std::numeric_limits<std::int64_t>::min(), high = std::numeric_limits<std::int64_t>::max();
    require(checkedArithmetic(OpCode::Mul, low, 1).value == low, "minimum * 1");
    require(checkedArithmetic(OpCode::Mul, low, 0).value == 0, "minimum * 0");
    require(!checkedArithmetic(OpCode::Mul, -1, low), "negative multiplication overflow");
    require(checkedArithmetic(OpCode::Sub, -1, low).value == high, "subtraction boundary");
    require(!checkedArithmetic(OpCode::Add, low, -1), "negative addition overflow");
    require(!checkedArithmetic(OpCode::Sub, high, -1), "positive subtraction overflow");
}

}
int main() {
    try { lexerTests(); parserTests(); semanticTests(); irTests(); bytecodeTests(); vmTests(); std::cout << checks << " checks passed\n"; return 0; }
    catch (const std::exception& e) { std::cerr << "FAILED: " << e.what() << "\n"; return 1; }
}

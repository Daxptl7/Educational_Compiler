#include "minic/bytecode.hpp"
#include <charconv>
#include <iomanip>
#include <limits>
#include <sstream>
namespace minic {
namespace {
constexpr const char* names[] = {"PUSH", "LOAD", "STORE", "ADD", "SUB", "MUL", "DIV", "MOD", "NEG", "NOT",
    "EQ", "NE", "LT", "LE", "GT", "GE", "JMP", "JZ", "PRINT", "HALT"};
bool hasOperand(OpCode op) {
    return op == OpCode::Push || op == OpCode::Load || op == OpCode::Store || op == OpCode::Jmp || op == OpCode::Jz;
}
bool validLocation(Location start, Location end) {
    return start.line > 0 && start.column > 0 && end.line > 0 && end.column > 0 &&
        (end.line > start.line || (end.line == start.line && end.column >= start.column));
}
std::string line(std::istream& in) {
    std::string result;
    char c;
    while (in.get(c)) {
        if (c == '\n') break;
        if (result.size() >= 4096) throw BytecodeError("bytecode line exceeds 4096 bytes");
        result += c;
    }
    if (in.bad()) throw BytecodeError("bytecode read failed");
    if (!result.empty() && result.back() == '\r') result.pop_back();
    return result;
}
std::vector<std::string> words(const std::string& text) {
    std::istringstream in(text); std::vector<std::string> result; std::string word;
    while (in >> word) result.push_back(word);
    return result;
}
template<class T> T number(const std::string& text) {
    T result{};
    const auto parsed = std::from_chars(text.data(), text.data() + text.size(), result);
    if (parsed.ec != std::errc{} || parsed.ptr != text.data() + text.size())
        throw BytecodeError("invalid numeric operand '" + text + "'");
    return result;
}
std::size_t countLine(std::istream& in, const std::string& name) {
    const auto w = words(line(in));
    if (w.size() != 2 || w[0] != name) throw BytecodeError("expected " + name + " count");
    const auto count = number<std::size_t>(w[1]);
    if (count > bytecodeResourceLimit) throw BytecodeError(name + " count exceeds resource limit");
    return count;
}
OpCode parseOpcode(const std::string& name) {
    for (std::size_t i = 0; i < sizeof(names) / sizeof(names[0]); ++i)
        if (name == names[i]) return static_cast<OpCode>(i);
    throw BytecodeError("unknown instruction '" + name + "'");
}
}
const char* opcodeName(OpCode op) {
    const auto i = static_cast<std::size_t>(op);
    if (i >= sizeof(names) / sizeof(names[0])) throw BytecodeError("invalid opcode");
    return names[i];
}
OpCode arithmeticOpcode(TokenKind op, bool unary) {
    if (unary) {
        if (op == TokenKind::Minus) return OpCode::Neg;
        if (op == TokenKind::Bang) return OpCode::Not;
    } else {
        switch (op) {
        case TokenKind::Plus: return OpCode::Add;
        case TokenKind::Minus: return OpCode::Sub;
        case TokenKind::Star: return OpCode::Mul;
        case TokenKind::Slash: return OpCode::Div;
        case TokenKind::Percent: return OpCode::Mod;
        case TokenKind::EqualEqual: return OpCode::Eq;
        case TokenKind::BangEqual: return OpCode::Ne;
        case TokenKind::Less: return OpCode::Lt;
        case TokenKind::LessEqual: return OpCode::Le;
        case TokenKind::Greater: return OpCode::Gt;
        case TokenKind::GreaterEqual: return OpCode::Ge;
        default: break;
        }
    }
    throw std::logic_error("invalid arithmetic operator");
}
void validateBytecode(const BytecodeProgram& p) {
    if (p.slotCount > bytecodeResourceLimit || p.instructions.empty() || p.instructions.size() > bytecodeResourceLimit)
        throw BytecodeError("invalid bytecode size or slot count");
    // SOURCE uses one quoted physical line; control characters are not filenames here.
    if (p.sourceFile.size() > 1800) throw BytecodeError("source filename too long");
    for (char raw : p.sourceFile) {
        const auto c = static_cast<unsigned char>(raw);
        if (c < 32 || c == 127) throw BytecodeError("invalid source filename");
    }
    for (const auto& i : p.instructions) {
        opcodeName(i.op);
        if (!validLocation(i.start, i.end)) throw BytecodeError("invalid source location");
        if (!hasOperand(i.op) && i.argument != 0) throw BytecodeError("unexpected instruction operand");
        if (i.op == OpCode::Load || i.op == OpCode::Store) {
            if (i.argument < 0 || static_cast<std::uint64_t>(i.argument) >= p.slotCount)
                throw BytecodeError("slot index out of bounds");
        }
        if (i.op == OpCode::Jmp || i.op == OpCode::Jz) {
            if (i.argument < 0 || static_cast<std::uint64_t>(i.argument) >= p.instructions.size())
                throw BytecodeError("jump target out of bounds");
        }
    }
}
BytecodeProgram BytecodeGenerator::generate(const IRProgram& ir, const SourceManager& source) const {
    BytecodeProgram p; p.sourceFile = source.filename();
    if (ir.symbols.size() > bytecodeResourceLimit || ir.temporaryCount > bytecodeResourceLimit - ir.symbols.size())
        throw BytecodeError("too many slots");
    p.slotCount = ir.symbols.size() + ir.temporaryCount;
    std::vector<std::size_t> labels(ir.labelCount, std::numeric_limits<std::size_t>::max());
    std::vector<std::pair<std::size_t, std::size_t>> fixups;
    auto emit = [&](OpCode op, std::int64_t argument, Span span) {
        p.instructions.push_back({op, argument, source.location(span.begin), source.location(span.end)});
    };
    auto slot = [&](const Operand& o) {
        if (o.kind == OperandKind::Symbol && o.id < ir.symbols.size()) return o.id;
        if (o.kind == OperandKind::Temporary && o.id < ir.temporaryCount) return ir.symbols.size() + o.id;
        throw std::logic_error("invalid IR slot");
    };
    auto load = [&](const Operand& o, Span span) {
        if (o.kind == OperandKind::Constant) emit(OpCode::Push, o.value, span);
        else emit(OpCode::Load, static_cast<std::int64_t>(slot(o)), span);
    };
    for (const auto& i : ir.instructions) {
        switch (i.kind) {
        case IRKind::Copy: case IRKind::Unary: case IRKind::Binary:
            load(i.left, i.span);
            if (i.kind == IRKind::Binary) { load(i.right, i.span); emit(arithmeticOpcode(i.op), 0, i.span); }
            if (i.kind == IRKind::Unary && i.op != TokenKind::Plus) emit(arithmeticOpcode(i.op, true), 0, i.span);
            emit(OpCode::Store, static_cast<std::int64_t>(slot(i.destination)), i.span); break;
        case IRKind::Label:
            if (i.label >= labels.size() || labels[i.label] != std::numeric_limits<std::size_t>::max())
                throw std::logic_error("invalid IR label definition");
            labels[i.label] = p.instructions.size(); break;
        case IRKind::Jump: case IRKind::JumpZero:
            if (i.kind == IRKind::JumpZero) load(i.left, i.span);
            fixups.emplace_back(p.instructions.size(), i.label);
            emit(i.kind == IRKind::Jump ? OpCode::Jmp : OpCode::Jz, 0, i.span); break;
        case IRKind::Print: load(i.left, i.span); emit(OpCode::Print, 0, i.span); break;
        case IRKind::Halt: emit(OpCode::Halt, 0, i.span); break;
        }
    }
    for (const auto& fixup : fixups) {
        if (fixup.second >= labels.size() || labels[fixup.second] == std::numeric_limits<std::size_t>::max())
            throw std::logic_error("undefined IR label");
        p.instructions[fixup.first].argument = static_cast<std::int64_t>(labels[fixup.second]);
    }
    validateBytecode(p); return p;
}
void BytecodeWriter::write(const BytecodeProgram& p, std::ostream& out) {
    validateBytecode(p);
    out << "MINIC 1\nSOURCE " << std::quoted(p.sourceFile) << "\nSLOTS " << p.slotCount
        << "\nCODE " << p.instructions.size() << '\n';
    for (const auto& i : p.instructions) {
        out << opcodeName(i.op);
        if (hasOperand(i.op)) out << ' ' << i.argument;
        out << " @ " << i.start.line << ' ' << i.start.column << ' ' << i.end.line << ' ' << i.end.column << '\n';
    }
    out << "END\n";
    if (!out) throw BytecodeError("bytecode write failed");
}
BytecodeProgram BytecodeReader::read(std::istream& in) {
    if (line(in) != "MINIC 1") throw BytecodeError("expected MINIC bytecode version 1");
    BytecodeProgram p;
    const auto source = line(in);
    if (source.compare(0, 8, "SOURCE \"") != 0) throw BytecodeError("expected quoted SOURCE filename");
    std::istringstream filename(source.substr(7));
    if (!(filename >> std::quoted(p.sourceFile))) throw BytecodeError("invalid SOURCE filename");
    filename >> std::ws;
    if (!filename.eof()) throw BytecodeError("unexpected SOURCE data");
    p.slotCount = countLine(in, "SLOTS");
    const auto count = countLine(in, "CODE");
    p.instructions.reserve(count);
    for (std::size_t n = 0; n < count; ++n) {
        const auto w = words(line(in));
        if (w.empty()) throw BytecodeError("missing instruction");
        BytecodeInstruction i; i.op = parseOpcode(w[0]);
        const std::size_t at = hasOperand(i.op) ? 2 : 1;
        if (w.size() != at + 5 || w[at] != "@") throw BytecodeError("invalid operand count or source metadata");
        if (hasOperand(i.op)) i.argument = number<std::int64_t>(w[1]);
        i.start = {number<std::size_t>(w[at + 1]), number<std::size_t>(w[at + 2])};
        i.end = {number<std::size_t>(w[at + 3]), number<std::size_t>(w[at + 4])};
        p.instructions.push_back(i);
    }
    if (line(in) != "END") throw BytecodeError("expected END after instructions");
    if (in.peek() != std::char_traits<char>::eof()) throw BytecodeError("unexpected data after END");
    if (in.bad()) throw BytecodeError("bytecode read failed");
    validateBytecode(p); return p;
}
void disassemble(const BytecodeProgram& p, std::ostream& out) {
    validateBytecode(p);
    for (std::size_t pc = 0; pc < p.instructions.size(); ++pc) {
        const auto& i = p.instructions[pc]; out << pc << ": " << opcodeName(i.op);
        if (hasOperand(i.op)) out << ' ' << i.argument;
        out << " @ " << i.start.line << ':' << i.start.column << '\n';
    }
}
}

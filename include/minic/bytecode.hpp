#pragma once
#include "minic/ir.hpp"
#include <istream>
#include <stdexcept>
namespace minic {
enum class OpCode {
    Push, Load, Store, Add, Sub, Mul, Div, Mod, Neg, Not,
    Eq, Ne, Lt, Le, Gt, Ge, Jmp, Jz, Print, Halt
};
struct BytecodeInstruction {
    OpCode op = OpCode::Halt;
    std::int64_t argument = 0;
    Location start, end;
};
struct BytecodeProgram {
    std::string sourceFile;
    std::size_t slotCount = 0;
    std::vector<BytecodeInstruction> instructions;
};
class BytecodeError : public std::runtime_error { using std::runtime_error::runtime_error; };
constexpr std::size_t bytecodeResourceLimit = 1000000;
const char* opcodeName(OpCode op);
OpCode arithmeticOpcode(TokenKind op, bool unary = false);
void validateBytecode(const BytecodeProgram& program);
class BytecodeGenerator {
public:
    BytecodeProgram generate(const IRProgram& ir, const SourceManager& source) const;
};
class BytecodeWriter {
public:
    static void write(const BytecodeProgram& program, std::ostream& out);
};
class BytecodeReader {
public:
    static BytecodeProgram read(std::istream& in);
};
void disassemble(const BytecodeProgram& program, std::ostream& out);
}

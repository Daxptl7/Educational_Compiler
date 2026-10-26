#pragma once
#include "minic/semantic.hpp"
namespace minic {
enum class OperandKind { Constant, Symbol, Temporary };
struct Operand {
    OperandKind kind = OperandKind::Constant;
    std::int64_t value = 0;
    std::size_t id = 0;
    static Operand constant(std::int64_t value) { return {OperandKind::Constant, value, 0}; }
    static Operand symbol(SymbolId id) { return {OperandKind::Symbol, 0, id}; }
    static Operand temporary(std::size_t id) { return {OperandKind::Temporary, 0, id}; }
};
enum class IRKind { Copy, Unary, Binary, Label, Jump, JumpZero, Print, Halt };
struct IRInstruction {
    IRKind kind = IRKind::Halt;
    Operand destination, left, right;
    TokenKind op = TokenKind::End;
    std::size_t label = 0;
    Span span;
};
struct IRProgram {
    std::vector<IRInstruction> instructions;
    std::vector<Symbol> symbols;
    std::size_t temporaryCount = 0, labelCount = 0;
};
class IRGenerator {
public:
    IRProgram generate(const BoundProgram& program);
private:
    IRProgram result_;
    Operand expression(const Expr& expr);
    void statement(const Stmt& stmt);
    void emit(IRKind kind, Span span, Operand destination = {}, Operand left = {},
              Operand right = {}, TokenKind op = TokenKind::End, std::size_t label = 0);
};
std::string operandText(const Operand& operand);
void dumpIR(const IRProgram& program, std::ostream& out);
}

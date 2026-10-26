#include "minic/ir.hpp"
#include <stdexcept>
namespace minic {
void IRGenerator::emit(IRKind kind, Span span, Operand destination, Operand left,
                       Operand right, TokenKind op, std::size_t label) {
    result_.instructions.push_back({kind, destination, left, right, op, label, span});
}
Operand IRGenerator::expression(const Expr& e) {
    if (e.kind == ExprKind::Literal) return Operand::constant(e.value);
    if (e.kind == ExprKind::Variable) {
        if (e.symbol == unresolved) throw std::logic_error("unbound IR reference");
        return Operand::symbol(e.symbol);
    }
    const auto left = expression(*e.left); // Sequenced before the right operand.
    Operand right;
    if (e.kind == ExprKind::Binary) right = expression(*e.right);
    const auto dest = Operand::temporary(result_.temporaryCount++);
    emit(e.kind == ExprKind::Unary ? IRKind::Unary : IRKind::Binary,
         e.operatorSpan, dest, left, right, e.op);
    return dest;
}
void IRGenerator::statement(const Stmt& s) {
    switch (s.kind) {
    case StmtKind::Declaration: case StmtKind::Assignment: {
        const auto value = expression(*s.expression);
        if (s.symbol == unresolved) throw std::logic_error("unbound IR destination");
        emit(IRKind::Copy, s.span, Operand::symbol(s.symbol), value); break;
    }
    case StmtKind::Print: {
        const auto value = expression(*s.expression);
        emit(IRKind::Print, s.span, {}, value); break;
    }
    case StmtKind::Block:
        for (const auto& child : s.items) statement(*child);
        break;
    case StmtKind::If: {
        const auto otherwise = result_.labelCount++;
        const auto end = s.alternative ? result_.labelCount++ : otherwise;
        const auto condition = expression(*s.expression);
        emit(IRKind::JumpZero, s.expression->span, {}, condition, {}, TokenKind::End, otherwise);
        statement(*s.body);
        if (s.alternative) emit(IRKind::Jump, s.span, {}, {}, {}, TokenKind::End, end);
        emit(IRKind::Label, s.span, {}, {}, {}, TokenKind::End, otherwise);
        if (s.alternative) {
            statement(*s.alternative);
            emit(IRKind::Label, s.span, {}, {}, {}, TokenKind::End, end);
        }
        break;
    }
    case StmtKind::While: {
        const auto header = result_.labelCount++, end = result_.labelCount++;
        emit(IRKind::Label, s.span, {}, {}, {}, TokenKind::End, header);
        const auto condition = expression(*s.expression); // Must stay after header.
        emit(IRKind::JumpZero, s.expression->span, {}, condition, {}, TokenKind::End, end);
        statement(*s.body);
        emit(IRKind::Jump, s.span, {}, {}, {}, TokenKind::End, header);
        emit(IRKind::Label, s.span, {}, {}, {}, TokenKind::End, end); break;
    }
    }
}
IRProgram IRGenerator::generate(const BoundProgram& p) {
    result_ = IRProgram{}; result_.symbols = p.symbols;
    for (const auto& s : p.ast.items) statement(*s);
    emit(IRKind::Halt, {}); return std::move(result_);
}
std::string operandText(const Operand& o) {
    if (o.kind == OperandKind::Constant) return std::to_string(o.value);
    return std::string(o.kind == OperandKind::Symbol ? "s" : "t") + std::to_string(o.id);
}
void dumpIR(const IRProgram& p, std::ostream& out) {
    for (const auto& s : p.symbols) out << "; s" << s.id << " = " << s.name << '\n';
    for (const auto& i : p.instructions) {
        const auto dest = operandText(i.destination), left = operandText(i.left), right = operandText(i.right);
        switch (i.kind) {
        case IRKind::Copy: out << dest << " = " << left; break;
        case IRKind::Unary: out << dest << " = " << tokenName(i.op) << ' ' << left; break;
        case IRKind::Binary: out << dest << " = " << left << ' ' << tokenName(i.op) << ' ' << right; break;
        case IRKind::Label: out << 'L' << i.label << ':'; break;
        case IRKind::Jump: out << "jump L" << i.label; break;
        case IRKind::JumpZero: out << "jump_zero " << left << ", L" << i.label; break;
        case IRKind::Print: out << "print " << left; break;
        case IRKind::Halt: out << "halt"; break;
        }
        out << '\n';
    }
}
}

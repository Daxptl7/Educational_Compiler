#include "minic/optimizer.hpp"
#include "minic/arithmetic.hpp"
#include <optional>
namespace minic {
void IROptimizer::optimize(IRProgram& p) const {
    // Each generated temporary has exactly one defining IR instruction.
    // Track only values derived entirely from literals; never track symbols.
    std::vector<std::optional<std::int64_t>> constants(p.temporaryCount);
    auto replace = [&](Operand& o) {
        if (o.kind == OperandKind::Temporary && constants.at(o.id))
            o = Operand::constant(*constants[o.id]);
    };
    for (auto& i : p.instructions) {
        switch (i.kind) {
        case IRKind::Copy: case IRKind::Unary: case IRKind::Binary: case IRKind::JumpZero: case IRKind::Print:
            replace(i.left); break;
        default: break;
        }
        if (i.kind == IRKind::Binary) replace(i.right);
        std::optional<std::int64_t> folded;
        if (i.kind == IRKind::Unary && i.left.kind == OperandKind::Constant) {
            if (i.op == TokenKind::Plus) folded = i.left.value;
            else {
                const auto result = checkedArithmetic(arithmeticOpcode(i.op, true), i.left.value);
                if (result) folded = result.value;
            }
        } else if (i.kind == IRKind::Binary && i.left.kind == OperandKind::Constant && i.right.kind == OperandKind::Constant) {
            const auto result = checkedArithmetic(arithmeticOpcode(i.op), i.left.value, i.right.value);
            if (result) folded = result.value;
        }
        if (folded) {
            i.kind = IRKind::Copy; i.left = Operand::constant(*folded);
            i.right = {}; i.op = TokenKind::End; // Keep the original source span.
        }
        if ((i.kind == IRKind::Copy || i.kind == IRKind::Unary || i.kind == IRKind::Binary) && i.destination.kind == OperandKind::Temporary) {
            auto& known = constants.at(i.destination.id);
            known.reset();
            if (i.kind == IRKind::Copy && i.left.kind == OperandKind::Constant) known = i.left.value;
        }
    }
}
}

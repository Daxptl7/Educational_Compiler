#include "minic/arithmetic.hpp"
#include <limits>
namespace minic {
ArithmeticResult checkedArithmetic(OpCode op, std::int64_t a, std::int64_t b) {
    constexpr auto low = std::numeric_limits<std::int64_t>::min();
    constexpr auto high = std::numeric_limits<std::int64_t>::max();
    const ArithmeticResult overflow{0, "integer overflow"};
    switch (op) {
    case OpCode::Add:
        if ((b > 0 && a > high - b) || (b < 0 && a < low - b)) return overflow;
        return {a + b, {}};
    case OpCode::Sub:
        if ((b > 0 && a < low + b) || (b < 0 && a > high + b)) return overflow;
        return {a - b, {}};
    case OpCode::Mul:
        if (a > 0) {
            if ((b > 0 && a > high / b) || (b < 0 && b < low / a)) return overflow;
        } else if (a < 0) {
            if ((b > 0 && a < low / b) || (b < 0 && a < high / b)) return overflow;
        }
        return {a * b, {}};
    case OpCode::Div: case OpCode::Mod:
        if (b == 0) return {0, op == OpCode::Div ? "division by zero" : "remainder by zero"};
        if (a == low && b == -1) return overflow;
        return {op == OpCode::Div ? a / b : a % b, {}};
    case OpCode::Neg:
        if (a == low) return overflow;
        return {-a, {}};
    case OpCode::Not: return {a == 0 ? 1 : 0, {}};
    case OpCode::Eq: return {a == b ? 1 : 0, {}};
    case OpCode::Ne: return {a != b ? 1 : 0, {}};
    case OpCode::Lt: return {a < b ? 1 : 0, {}};
    case OpCode::Le: return {a <= b ? 1 : 0, {}};
    case OpCode::Gt: return {a > b ? 1 : 0, {}};
    case OpCode::Ge: return {a >= b ? 1 : 0, {}};
    default: throw std::logic_error("non-arithmetic opcode");
    }
}
}

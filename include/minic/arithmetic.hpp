#pragma once
#include "minic/bytecode.hpp"
namespace minic {
struct ArithmeticResult {
    std::int64_t value = 0;
    std::string error;
    explicit operator bool() const { return error.empty(); }
};
ArithmeticResult checkedArithmetic(OpCode op, std::int64_t left, std::int64_t right = 0);
}

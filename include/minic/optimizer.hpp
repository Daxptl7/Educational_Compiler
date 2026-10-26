#pragma once
#include "minic/ir.hpp"
namespace minic {
class IROptimizer {
public:
    void optimize(IRProgram& program) const;
};
}

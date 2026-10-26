#include "minic/vm.hpp"
#include "minic/arithmetic.hpp"
namespace minic {
void VirtualMachine::execute(const BytecodeProgram& p, std::ostream& output,
                             std::ostream& trace, VMOptions options) const {
    validateBytecode(p);
    std::vector<std::optional<std::int64_t>> slots(p.slotCount);
    std::vector<std::int64_t> stack;
    std::size_t pc = 0;
    std::uint64_t steps = 0;
    Location current;
    auto fail = [&](const std::string& message) { throw RuntimeError(current, message); };
    auto pop = [&]() {
        if (stack.empty()) fail("stack underflow");
        const auto value = stack.back(); stack.pop_back(); return value;
    };
    auto push = [&](std::int64_t value) {
        if (stack.size() >= bytecodeResourceLimit) fail("operand stack resource limit exceeded");
        stack.push_back(value);
    };
    while (true) {
        if (pc >= p.instructions.size()) fail("instruction pointer fell outside bytecode (missing HALT)");
        const auto& i = p.instructions[pc]; current = i.start;
        if (options.maxSteps) {
            if (steps >= *options.maxSteps) fail("instruction limit exceeded");
            ++steps;
        }
        if (options.trace) {
            trace << pc << ": " << opcodeName(i.op);
            if (i.op == OpCode::Push || i.op == OpCode::Load || i.op == OpCode::Store || i.op == OpCode::Jmp || i.op == OpCode::Jz)
                trace << ' ' << i.argument;
            trace << " [stack=" << stack.size() << "] @ " << i.start.line << ':' << i.start.column << '\n';
        }
        ++pc;
        switch (i.op) {
        case OpCode::Push: push(i.argument); break;
        case OpCode::Load: {
            const auto& value = slots[static_cast<std::size_t>(i.argument)];
            if (!value) fail("read of uninitialized slot " + std::to_string(i.argument));
            push(*value); break;
        }
        case OpCode::Store: slots[static_cast<std::size_t>(i.argument)] = pop(); break;
        case OpCode::Jmp: pc = static_cast<std::size_t>(i.argument); break;
        case OpCode::Jz: if (pop() == 0) pc = static_cast<std::size_t>(i.argument); break;
        case OpCode::Print: output << pop() << '\n'; if (!output) fail("program output failed"); break;
        case OpCode::Halt:
            if (!stack.empty()) fail("nonempty operand stack at HALT");
            return;
        case OpCode::Neg: case OpCode::Not: {
            const auto result = checkedArithmetic(i.op, pop());
            if (!result) fail(result.error);
            push(result.value); break;
        }
        default: {
            const auto right = pop(); const auto left = pop();
            const auto result = checkedArithmetic(i.op, left, right);
            if (!result) fail(result.error);
            push(result.value); break;
        }
        }
    }
}
}

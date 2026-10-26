#pragma once
#include "minic/bytecode.hpp"
#include <optional>
namespace minic {
struct VMOptions {
    bool trace = false;
    std::optional<std::uint64_t> maxSteps;
};
class RuntimeError : public std::runtime_error {
public:
    RuntimeError(Location location, const std::string& message) : std::runtime_error(message), location_(location) {}
    Location location() const { return location_; }
private:
    Location location_;
};
class VirtualMachine {
public:
    void execute(const BytecodeProgram& program, std::ostream& output,
                 std::ostream& trace, VMOptions options = {}) const;
};
}

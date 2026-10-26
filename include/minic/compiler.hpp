#pragma once
#include "minic/bytecode.hpp"
namespace minic {
// Owns every frontend stage, so bound references remain valid until lowering.
class Compilation {
public:
    Compilation(std::string filename, std::string text);
    Compilation(const Compilation&) = delete;
    Compilation& operator=(const Compilation&) = delete;
    Compilation(Compilation&&) = delete;
    Compilation& operator=(Compilation&&) = delete;
    const SourceManager& source() const { return source_; }
    const DiagnosticEngine& diagnostics() const { return diagnostics_; }
    const std::vector<Token>& tokens() const { return tokens_; }
    const Program& ast() const { return ast_; }
    bool tokenize();
    bool parse();
    bool check();
    std::optional<IRProgram> lower(bool optimize);
private:
    SourceManager source_;
    DiagnosticEngine diagnostics_;
    std::vector<Token> tokens_;
    Program ast_;
    std::optional<BoundProgram> bound_;
    bool lexed_ = false, parsed_ = false, analyzed_ = false;
};
}

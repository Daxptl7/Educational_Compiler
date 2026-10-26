#include "minic/compiler.hpp"
#include "minic/optimizer.hpp"
#include "minic/parser.hpp"
namespace minic {
Compilation::Compilation(std::string filename, std::string text)
    : source_(std::move(filename), std::move(text)) {}
bool Compilation::tokenize() {
    if (!lexed_) { tokens_ = Lexer(source_, diagnostics_).scan(); lexed_ = true; }
    return !diagnostics_.hasErrors();
}
bool Compilation::parse() {
    if (!tokenize()) return false;
    if (!parsed_) { ast_ = Parser(tokens_, diagnostics_).parse(); parsed_ = true; }
    return !diagnostics_.hasErrors();
}
bool Compilation::check() {
    if (!parse()) return false;
    if (!analyzed_) {
        auto result = SemanticAnalyzer(diagnostics_).analyze(ast_);
        if (result) bound_.emplace(std::move(*result));
        analyzed_ = true;
    }
    return bound_.has_value();
}
std::optional<IRProgram> Compilation::lower(bool optimize) {
    if (!check()) return std::nullopt;
    auto ir = IRGenerator().generate(*bound_);
    if (optimize) IROptimizer().optimize(ir);
    return ir;
}
}

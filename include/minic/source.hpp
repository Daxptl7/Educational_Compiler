#pragma once
#include <cstddef>
#include <ostream>
#include <string>
#include <vector>
namespace minic {
struct Span { std::size_t begin = 0, end = 0; };
struct Location { std::size_t line = 1, column = 1; };
class SourceManager {
public:
    SourceManager(std::string filename, std::string text);
    const std::string& filename() const { return filename_; }
    const std::string& text() const { return text_; }
    Location location(std::size_t offset) const;
private:
    std::string filename_, text_;
    std::vector<std::size_t> lines_;
};
struct Diagnostic { Span span; std::string message; };
class DiagnosticEngine {
public:
    void error(Span span, std::string message);
    bool hasErrors() const { return !errors_.empty(); }
    const std::vector<Diagnostic>& errors() const { return errors_; }
    void print(const SourceManager& source, std::ostream& out) const;
private:
    std::vector<Diagnostic> errors_;
};
}

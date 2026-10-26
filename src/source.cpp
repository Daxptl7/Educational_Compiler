#include "minic/source.hpp"
#include <algorithm>
#include <utility>
namespace minic {
SourceManager::SourceManager(std::string filename, std::string text)
    : filename_(std::move(filename)), text_(std::move(text)), lines_{0} {
    for (std::size_t i = 0; i < text_.size(); ++i)
        if (text_[i] == '\n') lines_.push_back(i + 1);
}
Location SourceManager::location(std::size_t offset) const {
    offset = std::min(offset, text_.size());
    const auto line = static_cast<std::size_t>(
        std::upper_bound(lines_.begin(), lines_.end(), offset) - lines_.begin());
    return {line, offset - lines_[line - 1] + 1};
}
void DiagnosticEngine::error(Span span, std::string message) {
    errors_.push_back({span, std::move(message)});
}
void DiagnosticEngine::print(const SourceManager& source, std::ostream& out) const {
    for (const auto& d : errors_) {
        const auto loc = source.location(d.span.begin);
        out << source.filename() << ':' << loc.line << ':' << loc.column
            << ": error: " << d.message << '\n';
    }
}
}

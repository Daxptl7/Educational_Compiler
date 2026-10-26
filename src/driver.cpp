#include "minic/driver.hpp"
#include "minic/compiler.hpp"
#include "minic/vm.hpp"
#include <charconv>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
namespace minic {
namespace {
class UsageError : public std::runtime_error { using std::runtime_error::runtime_error; };
enum class Action { Tokens, AST, Check, IR, Emit, Run };
struct CompilerOptions {
    std::optional<Action> action;
    std::string input, output;
    bool optimize = false;
};
void compilerUsage(std::ostream& out) {
    out << "MiniC 1.0.0 - compiler targeting a custom stack VM\n"
        << "Usage: minic ACTION file.mc [-O0|-O1] [-o file.mbc]\n"
        << "Actions (choose exactly one):\n"
        << "  --tokens          Dump lexical tokens\n"
        << "  --ast             Dump syntax tree (before name resolution)\n"
        << "  --check           Check syntax and names\n"
        << "  --ir              Dump three-address IR\n"
        << "  --emit-bytecode   Write bytecode; requires -o\n"
        << "  --run             Compile through IR and execute bytecode\n"
        << "  --help            Show usage\n"
        << "  --version         Show version\n"
        << "Default optimization: -O0; -O1 enables safe constant folding.\n"
        << "Use -- before a filename beginning with '-'.\n"
        << "Exit codes: 0 success, 1 source error, 2 CLI/I/O/bytecode error, 3 runtime error.\n";
}
void vmUsage(std::ostream& out) {
    out << "Usage: minic-vm file.mbc [--trace] [--max-steps N]\n"
        << "  --trace        Trace instructions to stderr\n"
        << "  --max-steps N  Stop after N instructions (positive decimal integer)\n"
        << "  --help         Show usage\n"
        << "  --version      Show version\n"
        << "Program output goes to stdout. No instruction limit by default.\n";
}
CompilerOptions compilerOptions(int argc, char** argv) {
    CompilerOptions o; bool optimizationSeen = false, positional = false, outputSeen = false;
    for (int n = 1; n < argc; ++n) {
        const std::string arg = argv[n];
        if (!positional && arg == "--") { positional = true; continue; }
        if (!positional && (arg == "-O0" || arg == "-O1")) {
            if (optimizationSeen) throw UsageError("specify optimization only once");
            optimizationSeen = true; o.optimize = arg == "-O1";
        } else if (!positional && arg == "-o") {
            if (outputSeen || n + 1 == argc) throw UsageError("-o requires exactly one output filename");
            outputSeen = true; o.output = argv[++n];
            if (o.output.empty() || o.output[0] == '-') throw UsageError("invalid output filename after -o");
        } else if (!positional && !arg.empty() && arg[0] == '-') {
            std::optional<Action> action;
            if (arg == "--tokens") action = Action::Tokens;
            else if (arg == "--ast") action = Action::AST;
            else if (arg == "--check") action = Action::Check;
            else if (arg == "--ir") action = Action::IR;
            else if (arg == "--emit-bytecode") action = Action::Emit;
            else if (arg == "--run") action = Action::Run;
            else throw UsageError("unknown option '" + arg + "'");
            if (o.action) throw UsageError("conflicting action modes; choose exactly one");
            o.action = action;
        } else {
            if (!o.input.empty() || arg.empty()) throw UsageError("specify exactly one input file");
            o.input = arg;
        }
    }
    if (!o.action) throw UsageError("an explicit action mode is required");
    if (o.input.empty()) throw UsageError("an input file is required");
    if (*o.action == Action::Emit && !outputSeen) throw UsageError("--emit-bytecode requires -o file.mbc");
    if (*o.action != Action::Emit && outputSeen) throw UsageError("-o is only valid with --emit-bytecode");
    return o;
}
std::string readFile(const std::string& path) {
    std::error_code ec;
    if (std::filesystem::is_directory(path, ec))
        throw UsageError("cannot read input file '" + path + "': it is a directory");
    std::ifstream in(path, std::ios::binary);
    if (!in) throw UsageError("cannot open input file '" + path + "'");
    std::string text;
    char buffer[8192];
    while (in.read(buffer, sizeof(buffer)) || in.gcount() != 0)
        text.append(buffer, static_cast<std::size_t>(in.gcount()));
    if (in.bad() || !in.eof()) throw UsageError("cannot read input file '" + path + "'");
    return text;
}
void writeFile(const std::string& path, const std::string& text) {
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out) throw UsageError("cannot open output file '" + path + "'");
    out << text; out.close();
    if (!out) throw UsageError("cannot write output file '" + path + "'");
}
void runtimeDiagnostic(const std::string& filename, const RuntimeError& e) {
    std::cerr << filename << ':' << e.location().line << ':' << e.location().column << ": runtime error: " << e.what() << '\n';
}
std::uint64_t stepCount(const std::string& text) {
    std::uint64_t count = 0;
    const auto result = std::from_chars(text.data(), text.data() + text.size(), count);
    if (result.ec != std::errc{} || result.ptr != text.data() + text.size() || count == 0)
        throw UsageError("--max-steps requires a positive decimal integer");
    return count;
}
}
int compilerMain(int argc, char** argv) {
    if (argc == 2 && std::string(argv[1]) == "--help") { compilerUsage(std::cout); return 0; }
    if (argc == 2 && std::string(argv[1]) == "--version") { std::cout << "MiniC 1.0.0\n"; return 0; }
    std::string input;
    try {
        const auto options = compilerOptions(argc, argv); input = options.input;
        Compilation compilation(input, readFile(input));
        const auto action = *options.action;
        bool success;
        if (action == Action::Tokens) success = compilation.tokenize();
        else if (action == Action::AST) success = compilation.parse();
        else success = compilation.check();
        if (!success) { compilation.diagnostics().print(compilation.source(), std::cerr); return 1; }
        if (action == Action::Tokens) dumpTokens(compilation.tokens(), compilation.source(), std::cout);
        else if (action == Action::AST) dumpAST(compilation.ast(), std::cout);
        else if (action != Action::Check) {
            const auto ir = compilation.lower(options.optimize);
            if (!ir) throw std::logic_error("checked compilation failed to lower");
            if (action == Action::IR) dumpIR(*ir, std::cout);
            else {
                const auto p = BytecodeGenerator().generate(*ir, compilation.source());
                if (action == Action::Run) VirtualMachine().execute(p, std::cout, std::cerr);
                else {
                    // Finish compilation and serialization before opening any output file.
                    std::ostringstream encoded; BytecodeWriter::write(p, encoded);
                    if (std::filesystem::weakly_canonical(input) == std::filesystem::weakly_canonical(options.output))
                        throw UsageError("output file must differ from the source file");
                    std::error_code ec;
                    if (std::filesystem::equivalent(input, options.output, ec) && !ec)
                        throw UsageError("output file must differ from the source file");
                    writeFile(options.output, encoded.str());
                }
            }
        }
        if (!std::cout) throw UsageError("output write failed");
        return 0;
    } catch (const RuntimeError& e) { runtimeDiagnostic(input, e); return 3; }
    catch (const UsageError& e) { std::cerr << "minic: " << e.what() << "\nSee minic --help for usage.\n"; return 2; }
    catch (const std::exception& e) { std::cerr << "minic: " << e.what() << '\n'; return 2; }
}
int vmMain(int argc, char** argv) {
    if (argc == 2 && std::string(argv[1]) == "--help") { vmUsage(std::cout); return 0; }
    if (argc == 2 && std::string(argv[1]) == "--version") { std::cout << "MiniC VM 1.0.0\n"; return 0; }
    std::string sourceFile;
    try {
        std::string input; VMOptions options; bool positional = false, limitSeen = false;
        for (int n = 1; n < argc; ++n) {
            const std::string arg = argv[n];
            if (!positional && arg == "--") { positional = true; continue; }
            if (!positional && arg == "--trace") {
                if (options.trace) throw UsageError("duplicate --trace");
                options.trace = true;
            } else if (!positional && arg == "--max-steps") {
                if (limitSeen || n + 1 == argc) throw UsageError("--max-steps requires one limit");
                limitSeen = true; options.maxSteps = stepCount(argv[++n]);
            } else if (!positional && !arg.empty() && arg[0] == '-') throw UsageError("unknown option '" + arg + "'");
            else {
                if (!input.empty() || arg.empty()) throw UsageError("specify exactly one bytecode file");
                input = arg;
            }
        }
        if (input.empty()) throw UsageError("a bytecode file is required");
        std::ifstream in(input, std::ios::binary);
        if (!in) throw UsageError("cannot open input file '" + input + "'");
        const auto p = BytecodeReader::read(in); sourceFile = p.sourceFile;
        VirtualMachine().execute(p, std::cout, std::cerr, options); return 0;
    } catch (const RuntimeError& e) { runtimeDiagnostic(sourceFile, e); return 3; }
    catch (const UsageError& e) { std::cerr << "minic-vm: " << e.what() << "\nSee minic-vm --help for usage.\n"; return 2; }
    catch (const std::exception& e) { std::cerr << "minic-vm: " << e.what() << '\n'; return 2; }
}
}

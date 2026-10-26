#include "minic/driver.hpp"
#include <iostream>
#include <string>
namespace minic {
int compilerMain(int argc, char** argv) {
    if (argc == 2 && std::string(argv[1]) == "--help") {
        std::cout << "MiniC: --tokens | --ast | --check | --ir | --emit-bytecode | --run file.mc [-O0|-O1] [-o file.mbc]\n";
        return 0;
    }
    if (argc == 2 && std::string(argv[1]) == "--version") {
        std::cout << "MiniC 1.0.0\n"; return 0;
    }
    std::cerr << "MiniC: compiler stages are under construction; see --help\n";
    return 2;
}
int vmMain(int argc, char** argv) {
    if (argc == 2 && std::string(argv[1]) == "--help") {
        std::cout << "minic-vm file.mbc [--trace] [--max-steps N]\n"; return 0;
    }
    std::cerr << "minic-vm: VM is under construction; see --help\n";
    return 2;
}
}

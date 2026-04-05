#include "MLabEngine.hpp"

#include <iostream>
#include <string>

int main()
{
    mlab::Engine engine;

    std::cout << "MLab Interpreter\n";
    std::cout << "Type 'quit' or 'exit' to leave.\n\n";

    std::string line;
    while (true) {
        std::cout << ">> ";
        if (!std::getline(std::cin, line))
            break;
        if (line == "quit" || line == "exit")
            break;
        if (line.empty())
            continue;
        auto r = engine.evalSafe(line);
        if (!r.ok) {
            if (r.errorLine > 0)
                std::cerr << "Error at line " << r.errorLine
                          << ", column " << r.errorCol << ":\n  "
                          << r.errorMessage << "\n";
            else
                std::cerr << "Error: " << r.errorMessage << "\n";
        }
    }

    std::cout << "\nGoodbye!\n";
    return 0;
}
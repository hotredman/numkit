#include <numkit/m/core/MEngine.hpp>

#include <iostream>
#include <string>

int main()
{
    numkit::m::Engine engine;

    std::cout << "M Interpreter\n";
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
            std::string ctx = r.errorContext.empty() ? "" : " (" + r.errorContext + ")";
            if (r.errorLine > 0)
                std::cerr << "Error at line " << r.errorLine
                          << ", column " << r.errorCol << ":\n  "
                          << r.errorMessage << ctx << "\n";
            else
                std::cerr << "Error: " << r.errorMessage << ctx << "\n";
        }
    }

    std::cout << "\nGoodbye!\n";
    return 0;
}
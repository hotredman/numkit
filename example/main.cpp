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
        try {
            engine.eval(line);
        } catch (const std::exception &e) {
            std::cerr << "Error: " << e.what() << "\n";
        }
    }

    std::cout << "\nGoodbye!\n";
    return 0;
}
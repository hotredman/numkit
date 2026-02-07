#include "MLabEngine.hpp"
#include "MLabStdLibrary.hpp"

#include <iostream>
#include <string>

int main(int argc, char *argv[])
{
    mlab::Engine engine;

    // Custom allocator with tracking
    size_t totalAllocated = 0;
    engine.setAllocator({[&](size_t n) -> void * {
                             totalAllocated += n;
                             return ::operator new(n);
                         },
                         [&](void *p, size_t n) {
                             totalAllocated -= n;
                             ::operator delete(p);
                         }});

    mlab::StdLibrary::install(engine);

    std::cout << "=== MATLAB Interpreter ===\n\n";

    try {
        // Arithmetic
        engine.eval("x = 3 + 4 * 2;");
        engine.eval("disp(x)");

        // Matrices
        engine.eval("A = [1 2 3; 4 5 6; 7 8 9]");
        engine.eval("disp(A')");
        engine.eval("disp(A * A')");

        // For loop
        engine.eval(R"(
            s = 0;
            for i = 1:10
                s = s + i;
            end
            disp(s)
        )");

        // User function
        engine.eval(R"(
            function y = factorial(n)
                if n <= 1
                    y = 1;
                else
                    y = n * factorial(n - 1);
                end
            end
        )");
        engine.eval("disp(factorial(10))");

        // Complex numbers
        engine.eval("z = 3 + 4i");
        engine.eval("disp(abs(z))");
        engine.eval("disp(real(z))");
        engine.eval("disp(imag(z))");

        // String functions
        engine.eval("disp(upper('hello'))");
        engine.eval("disp(strcmp('abc', 'abc'))");

        // Memory
        std::cout << "\nMemory allocated: " << totalAllocated << " bytes\n";

    } catch (const std::exception &e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    // REPL
    if (argc > 1 && std::string(argv[1]) == "--repl") {
        std::cout << "\n=== REPL (type 'quit' to exit) ===\n";
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
    }

    return 0;
}

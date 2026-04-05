% Factorial — Recursive factorial function
% Demonstrates recursion and base case handling.
clear

function y = factorial(n)
    if n <= 1
        y = 1;
    else
        y = n * factorial(n - 1);
    end
end

disp(factorial(10))

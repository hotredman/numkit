% Factorial — Recursive factorial function
% Demonstrates recursion and base case handling.
% Pull toolbox functions (signal, stats, graphics, io) into scope so we
% can call them by short name (fft, butter, plot, std, ...). Without this,
% we'd need fully qualified names like signal.transforms.fft(...).
import compat.*;

clear

function y = factorial(n)
    if n <= 1
        y = 1;
    else
        y = n * factorial(n - 1);
    end
end

disp(factorial(10))

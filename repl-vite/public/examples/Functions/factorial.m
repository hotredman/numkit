% Factorial — Recursive factorial function
function y = factorial(n)
    if n <= 1
        y = 1;
    else
        y = n * factorial(n - 1);
    end
end

disp(factorial(10))

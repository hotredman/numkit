% Fibonacci — Recursive Fibonacci sequence
% Compute the first 10 Fibonacci numbers using recursion.
clear

function r = fib(n)
    if n <= 1
        r = n;
    else
        r = fib(n-1) + fib(n-2);
    end
end

result = [];
for k = 0:9
    result = [result, fib(k)];
end
disp(result)

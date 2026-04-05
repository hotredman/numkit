% Benchmark — Interpreter performance tests
% Measures raw interpreter speed: loops, function calls,
% indexing, and scalar math. Does NOT test library functions
% like fft or inv — only the execution engine itself.
clear

fprintf('\n=== Interpreter Benchmark ===\n\n')

% ── 1. Nested loop overhead ────────────────────────────────
% Pure loop iteration with minimal body.
% This is the most direct measure of loop dispatch cost.
N1 = 300;
tic
s = 0;
for i = 1:N1
    for j = 1:N1
        s = s + 1;
    end
end
t1 = toc;
fprintf('1. Nested loop %dx%d        : %.4f s  (sum=%d)\n', N1, N1, t1, s)

% ── 2. Scalar arithmetic in loop ───────────────────────────
% Tests expression evaluation overhead per iteration.
N2 = 50000;
tic
x = 0;
for i = 1:N2
    x = x + i * 0.5 - 1.0 / (i + 1);
end
t2 = toc;
fprintf('2. Scalar math (%d iters)  : %.4f s  (x=%.4f)\n', N2, t2, x)

% ── 3. Function call overhead ──────────────────────────────
% Repeatedly call a trivial user function to measure
% call/return dispatch cost.
function y = add_one(x)
    y = x + 1;
end

N3 = 10000;
tic
v = 0;
for i = 1:N3
    v = add_one(v);
end
t3 = toc;
fprintf('3. Function calls (%d)    : %.4f s  (v=%d)\n', N3, t3, v)

% ── 4. Recursive function calls ────────────────────────────
% Recursion stresses call stack management.
function r = fib(n)
    if n <= 1
        r = n;
    else
        r = fib(n-1) + fib(n-2);
    end
end

tic
f = fib(20);
t4 = toc;
fprintf('4. Recursive fib(20)         : %.4f s  (fib=%d)\n', t4, f)

% ── 5. Array element write (growing) ──────────────────────
% Append elements one by one — worst case for dynamic arrays.
N5 = 10000;
tic
A = [];
for i = 1:N5
    A = [A, i];
end
t5 = toc;
fprintf('5. Array grow (%d)         : %.4f s  (len=%d)\n', N5, t5, length(A))

% ── 6. Array indexing read/write ───────────────────────────
% Pre-allocated array, random-access pattern.
N6 = 50000;
B = zeros(1, N6);
tic
for i = 1:N6
    B(i) = i * 2.5;
end
total = 0;
for i = 1:N6
    total = total + B(i);
end
t6 = toc;
fprintf('6. Array index r/w (%d)   : %.4f s  (sum=%.1f)\n', N6, t6, total)

% ── 7. Matrix element-wise nested loop ─────────────────────
% Double loop over a matrix — classic slow pattern.
N7 = 100;
M = zeros(N7, N7);
tic
for i = 1:N7
    for j = 1:N7
        M(i, j) = sin(i * 0.01) * cos(j * 0.01);
    end
end
t7 = toc;
fprintf('7. Matrix fill %dx%d       : %.4f s\n', N7, N7, t7)

% ── 8. String concatenation in loop ────────────────────────
N8 = 5000;
tic
str = '';
for i = 1:N8
    str = [str, 'a'];
end
t8 = toc;
fprintf('8. String concat (%d)      : %.4f s  (len=%d)\n', N8, t8, length(str))

% ── 9. Struct field access ─────────────────────────────────
N9 = 10000;
p.x = 0;
p.y = 0;
tic
for i = 1:N9
    p.x = p.x + 1;
    p.y = p.y + p.x;
end
t9 = toc;
fprintf('9. Struct field access (%d): %.4f s  (y=%.0f)\n', N9, t9, p.y)

% ── 10. Conditional branching ──────────────────────────────
N10 = 50000;
tic
c = 0;
for i = 1:N10
    if mod(i, 3) == 0
        c = c + 1;
    elseif mod(i, 5) == 0
        c = c + 2;
    else
        c = c + 3;
    end
end
t10 = toc;
fprintf('10. Branching (%d)        : %.4f s  (c=%d)\n', N10, t10, c)

% ── Summary ────────────────────────────────────────────────
total_time = t1+t2+t3+t4+t5+t6+t7+t8+t9+t10;
fprintf('\n── Total: %.4f s ──\n', total_time)
clear
import compat.*

% corr two-arg form (closes ⚠️ gap from cycle 84).
% Reference: MATLAB R2025b.

X = [1 2; 2 1; 3 3; 4 4; 5 5];
Y = [10 100; 20 200; 30 300; 40 400; 50 500];

C = corr(X);
fprintf('corr(X):\n'); disp(C);

C = corr(X, Y);
fprintf('corr(X, Y):\n'); disp(C);
fprintf('  (e [1 1; 0.9 0.9])\n');

c = corr([1;2;3;4;5], [2;4;6;8;10]);
fprintf('perfect linear corr: %g (e 1)\n', c);

c = corr([1;2;3;4;5], [5;4;3;2;1]);
fprintf('anti-correlated:     %g (e -1)\n', c);

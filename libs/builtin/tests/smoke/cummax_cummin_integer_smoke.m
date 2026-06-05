clear

import compat.*

% bugs/builtin/cummax-cummin-integer.md — cummax/cummin on integer input.
% MATLAB PRESERVES the integer class (order statistics).

a = cummax(int8([3 1 2 5 4]));
fprintf('cummax(int8([3 1 2 5 4])) = [%d %d %d %d %d]   expect [3 3 3 5 5], isa int8=%d (expect 1)\n', ...
        a(1), a(2), a(3), a(4), a(5), isa(a,'int8'));

b = cummin(int8([3 1 2 5 4]));
fprintf('cummin(int8([3 1 2 5 4])) = [%d %d %d %d %d]   expect [3 1 1 1 1]\n', b(1), b(2), b(3), b(4), b(5));

k = cummax(uint16([30 10 50 20]));
fprintf('cummax(uint16([30 10 50 20])) = [%d %d %d %d]   expect [30 30 50 50], isa uint16=%d (expect 1)\n', ...
        k(1), k(2), k(3), k(4), isa(k,'uint16'));

c = cummax(int8([3 1; 1 5]), 2);
fprintf('cummax(int8([3 1;1 5]),2) = [%d %d; %d %d]   expect [3 3; 1 5]\n', c(1,1), c(1,2), c(2,1), c(2,2));

r = cummax(int8([3 1 2 5 4]), 'reverse');
fprintf('cummax(...,''reverse'') = [%d %d %d %d %d]   expect [5 5 5 5 4]\n', r(1), r(2), r(3), r(4), r(5));

e = cummin(int8([0 -3 2 -5]));
fprintf('cummin(int8([0 -3 2 -5])) = [%d %d %d %d]   expect [0 -3 -3 -5]\n', e(1), e(2), e(3), e(4));

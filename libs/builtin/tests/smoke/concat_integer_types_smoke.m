clear

import compat.*

% bugs/builtin/concat-integer-types.md — concatenation of integer arrays.
% MATLAB R2025b: the FIRST integer operand's class wins; double/logical/a
% different int class/the real part of complex are cast (round-half-away +
% saturate). Previously numkit threw "Concatenation not supported for type".

a = [int8([1 2]); int8([3 4])];
fprintf('[int8;int8] = [%d %d; %d %d] class=%s   expect [1 2;3 4] int8\n', a(1,1),a(1,2),a(2,1),a(2,2), class(a));

b = [int8([1 2]), int8([3 4])];
fprintf('[int8,int8] = [%d %d %d %d] class=%s   expect [1 2 3 4] int8\n', b(1),b(2),b(3),b(4), class(b));

c = cat(1, uint16([10 20]), uint16([30 40]));
fprintf('cat(1,uint16) class=%s c(2,2)=%d   expect uint16 / 40\n', class(c), c(2,2));

d = [int8(5); 50.6];
fprintf('[int8(5);50.6] = [%d %d]   expect [5 51] (round)\n', d(1), d(2));
sa = [int8(5); 300];   sl = [int8(5); -300];
fprintf('[int8(5);300]=%d (sat 127), [int8(5);-300]=%d (sat -128)\n', sa(2), sl(2));

g = [int8(5); true];   fprintf('[int8;true] g(2)=%d class=%s (expect 1 int8)\n', g(2), class(g));
m = [int8(5), int16(6)]; fprintf('[int8,int16] class=%s (expect int8, first wins) m(2)=%d\n', class(m), m(2));
z = [int8(5); 2+3i];   fprintf('[int8;2+3i] z(2)=%d class=%s (expect 2 int8, real part)\n', z(2), class(z));

% Regression: double/logical/complex/char concat unchanged.
fprintf('regress: [1 2;3 4]=%s, [true false]=%s, [1+2i 3]=%s, [''ab'';''cd'']=%s\n', ...
        class([1 2;3 4]), class([true false]), class([1+2i 3]), class(['ab';'cd']));

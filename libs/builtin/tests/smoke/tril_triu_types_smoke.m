clear
import compat.*
% tril / triu keep one triangle and zero-fill the rest, type-preserving.
% DEEP-PROBE 2026-05-31: the 2-D/3-D path was DOUBLE-only (threw
% "Not a double array" on char/logical/single/complex); only the ndim>=4
% fallback already used the type-agnostic byte kernel.

M = ['abc';'def';'ghi'];
T = tril(M);
fprintf('tril(char): class=%s (expect char) size=%dx%d (expect 3x3)\n', class(T), size(T,1), size(T,2));
fprintf('  T(1,1)=%c (expect a) T(2,1)=%c (expect d) double(T(1,2))=%d (expect 0)\n', T(1,1), T(2,1), double(T(1,2)));
U = triu(M);
fprintf('triu(char): U(1,2)=%c (expect b) double(U(2,1))=%d (expect 0) U(1,1)=%c (expect a)\n', U(1,2), double(U(2,1)), U(1,1));

% k offset
Tk = tril(M, 1);
fprintf('tril(char,1): double(Tk(1,3))=%d (expect 0) Tk(3,1)=%c (expect g)\n', double(Tk(1,3)), Tk(3,1));
Uk = triu(M, -1);
fprintf('triu(char,-1): Uk(2,1)=%c (expect d) double(Uk(3,1))=%d (expect 0)\n', Uk(2,1), double(Uk(3,1)));

% LOGICAL / SINGLE / COMPLEX
L = tril(logical(ones(3)));
fprintf('tril(logical): islogical=%d (expect 1) L(2,1)=%d (expect 1) L(1,2)=%d (expect 0)\n', islogical(L), L(2,1), L(1,2));
S = tril(single([1 2;3 4]));
fprintf('tril(single): class=%s (expect single) S(2,1)=%g (expect 3) S(1,2)=%g (expect 0)\n', class(S), S(2,1), S(1,2));
Z = tril([1+1i 2+2i;3+3i 4+4i]);
fprintf('tril(complex): Z(2,1)=%s (expect 3+3i) Z(1,2)=%s (expect 0)\n', mat2str(Z(2,1)), mat2str(Z(1,2)));

% DOUBLE unchanged
D = tril([1 2;3 4]);
fprintf('tril(double): D(2,1)=%g (expect 3) D(1,2)=%g (expect 0)\n', D(2,1), D(1,2));

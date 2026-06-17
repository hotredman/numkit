clear
import compat.*
% blkdiag places each block on the diagonal, zero-filling the rest.
% Type-preserving. DEEP-PROBE 2026-05-31: blkdiag was DOUBLE-only (threw
% "Not a double array" on char/logical/single/complex blocks).

C = blkdiag('ab','cd');
fprintf('blkdiag(char): class=%s (expect char) size=%dx%d (expect 2x4)\n', class(C), size(C,1), size(C,2));
fprintf('  C(1,1)=%c (expect a) C(1,2)=%c (expect b) C(2,3)=%c (expect c) double(C(1,3))=%d (expect 0)\n', C(1,1), C(1,2), C(2,3), double(C(1,3)));

L = blkdiag(logical([1 1]), logical([0 1]));
fprintf('blkdiag(logical): islogical=%d (expect 1) L(1,1)=%d (expect 1) L(2,3)=%d (expect 0) L(1,3)=%d (expect 0)\n', ...
        islogical(L), L(1,1), L(2,3), L(1,3));

S = blkdiag(single([1 2]), single(3));
fprintf('blkdiag(single): class=%s (expect single) S(2,3)=%g (expect 3)\n', class(S), S(2,3));

Z = blkdiag([1+1i 2], 3+3i);
fprintf('blkdiag(complex): Z(1,1)=%s (expect 1+1i) Z(2,3)=%s (expect 3+3i) Z(1,3)=%s (expect 0)\n', ...
        mat2str(Z(1,1)), mat2str(Z(2,3)), mat2str(Z(1,3)));

D = blkdiag([1 2;3 4], 5);
fprintf('blkdiag(double): D(1,1)=%g (expect 1) D(3,3)=%g (expect 5) D(1,3)=%g (expect 0)\n', D(1,1), D(3,3), D(1,3));

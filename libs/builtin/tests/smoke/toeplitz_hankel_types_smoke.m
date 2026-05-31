clear
import compat.*
% toeplitz / hankel preserve COMPLEX and SINGLE inputs.
% DEEP-PROBE 2026-05-31: both were DOUBLE-only — they dropped the imaginary
% part of complex inputs and down-converted single to double.

% Single-arg COMPLEX toeplitz is Hermitian (lower triangle conjugated).
c = [1+1i 2+2i 3+3i];
T = toeplitz(c);
fprintf('toeplitz(c complex 1-arg, Hermitian):\n');
fprintf('  T(1,1)=%s (expect 1+1i) T(2,1)=%s (expect 2-2i, conj) T(1,2)=%s (expect 2+2i)\n', ...
        mat2str(T(1,1)), mat2str(T(2,1)), mat2str(T(1,2)));

% Two-arg COMPLEX toeplitz: plain gather, no conjugation.
T2 = toeplitz([1+1i 2 3], [1-9i 8 7]);
fprintf('toeplitz(c,r complex): T(1,1)=%s (expect 1+1i, col wins) T(2,1)=%s (expect 2) T(1,2)=%s (expect 8)\n', ...
        mat2str(T2(1,1)), mat2str(T2(2,1)), mat2str(T2(1,2)));

% SINGLE preserved.
Ts = toeplitz(single([1 2 3]));
fprintf('toeplitz(single): class=%s (expect single)\n', class(Ts));

% hankel COMPLEX (never conjugates).
H = hankel([1+1i 2+2i 3+3i]);
fprintf('hankel(c complex): H(1,1)=%s (expect 1+1i) H(2,2)=%s (expect 3+3i)\n', ...
        mat2str(H(1,1)), mat2str(H(2,2)));
H2 = hankel([1+1i 2 3], [3 4 5+5i]);
fprintf('hankel(c,r complex): H(3,3)=%s (expect 5+5i)\n', mat2str(H2(3,3)));
Hs = hankel(single([1 2 3]));
fprintf('hankel(single): class=%s (expect single)\n', class(Hs));

% DOUBLE unchanged.
Td = toeplitz([1 2 3], [1 4 5]);
fprintf('toeplitz(double): T(2,1)=%g (expect 2) T(1,2)=%g (expect 4)\n', Td(2,1), Td(1,2));

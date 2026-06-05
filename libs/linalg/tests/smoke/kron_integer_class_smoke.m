clear

import compat.*

% bugs/linalg/kron-integer-class.md — kron preserves the integer class of
% integer operands (MATLAB R2025b), with saturating round-half-away casts.
% Previously numkit always returned a double from kron.

k1 = kron(int8([1 2]), int8([1 1]));
fprintf('kron(int8,int8) = [%d %d %d %d] class=%s   expect [1 1 2 2] int8\n', ...
        k1(1), k1(2), k1(3), k1(4), class(k1));

s1 = kron(int8(100), int8(2));
fprintf('kron(int8(100),int8(2)) = %d class=%s   expect 127 int8 (sat hi)\n', s1, class(s1));

s2 = kron(uint8(200), uint8(2));
fprintf('kron(uint8(200),uint8(2)) = %d class=%s   expect 255 uint8 (sat hi)\n', s2, class(s2));

s3 = kron(int8(-100), int8(2));
fprintf('kron(int8(-100),int8(2)) = %d class=%s   expect -128 int8 (sat lo)\n', s3, class(s3));

ks = kron(int8([2 3]), 2);
fprintf('kron(int8([2 3]),2) = [%d %d] class=%s   expect [4 6] int8 (scalar cast)\n', ks(1), ks(2), class(ks));

kf = kron(int8(2), 1.5);
fprintf('kron(int8(2),1.5) = %d class=%s   expect 3 int8 (round-half-away)\n', kf, class(kf));

% Regression: double*double unchanged.
kd = kron([1 2], [3 4]);
fprintf('kron([1 2],[3 4]) = [%d %d %d %d] class=%s   expect [3 4 6 8] double\n', ...
        kd(1), kd(2), kd(3), kd(4), class(kd));

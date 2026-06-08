clear

import compat.*

% bugs/linalg/cross-integer-class.md — cross preserves the integer class of
% integer operands, with MATLAB R2025b's PER-OPERATION saturating integer
% arithmetic (each product saturates before the subtraction). Previously
% numkit threw "Not a double array" on any integer operand.

c1 = cross(int8([1 2 3]), int8([4 5 6]));
fprintf('cross(int8,int8) = [%g %g %g] class=%s   expect [-3 6 -3] int8\n', c1(1), c1(2), c1(3), class(c1));

c2 = cross(int32([1 0 0]), int32([0 1 0]));
fprintf('cross(int32 unit) = [%g %g %g] class=%s   expect [0 0 1] int32\n', c2(1), c2(2), c2(3), class(c2));

% Per-operation saturation: 100*100 saturates to 127 BEFORE the subtraction,
% so component 2 = 0 - 127 = -127 (not -128).
cs = cross(int8([100 100 0]), int8([0 100 100]));
fprintf('cross sat = [%g %g %g] class=%s   expect [127 -127 127] int8 (NOT -128)\n', cs(1), cs(2), cs(3), class(cs));

cu = cross(uint8([1 2 3]), uint8([4 5 6]));
fprintf('cross(uint8) = [%g %g %g] class=%s   expect [0 6 0] uint8 (neg clamps to 0)\n', cu(1), cu(2), cu(3), class(cu));

cd = cross(int8([1 2 3]), [4 5 6]);
fprintf('cross(int8,double) = [%g %g %g] class=%s   expect [-3 6 -3] int8 (int+double)\n', cd(1), cd(2), cd(3), class(cd));

c16 = cross(int16([10 20 30]), int16([40 50 60]));
fprintf('cross(int16) = [%g %g %g] class=%s   expect [-300 600 -300] int16\n', c16(1), c16(2), c16(3), class(c16));

% Regression: double*double unchanged.
dd = cross([1 0 0], [0 1 0]);
fprintf('cross(double) = [%g %g %g] class=%s   expect [0 0 1] double\n', dd(1), dd(2), dd(3), class(dd));

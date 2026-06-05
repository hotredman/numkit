clear

import compat.*

% bugs/signal/conv-integer-input.md — conv accepts integer/logical input.
% MATLAB R2025b promotes to double; the convolution result is ALWAYS double
% (never the integer class, unlike kron/cross). Previously numkit threw
% "Not a double array" on any integer or logical operand.

cf = conv(int8([1 2 3]), int8([1 1]));
fprintf('conv(int8,int8) = [%g %g %g %g] class=%s   expect [1 3 5 3] double\n', ...
        cf(1), cf(2), cf(3), cf(4), class(cf));

cs = conv(int8([1 2 3]), int8([1 1]), 'same');
fprintf('conv(int8,int8,''same'')  = [%g %g %g] class=%s   expect [3 5 3] double\n', cs(1), cs(2), cs(3), class(cs));

cv = conv(int8([1 2 3]), int8([1 1]), 'valid');
fprintf('conv(int8,int8,''valid'') = [%g %g] class=%s   expect [3 5] double\n', cv(1), cv(2), class(cv));

cm = conv(int8([1 2 3]), [1 1]);
fprintf('conv(int8,double) = [%g %g %g %g] class=%s   expect [1 3 5 3] double (int+double)\n', cm(1), cm(2), cm(3), cm(4), class(cm));

cw = conv(int16([100 200]), int16([2 2]));
fprintf('conv(int16,int16) = [%g %g %g] class=%s   expect [200 600 400] double\n', cw(1), cw(2), cw(3), class(cw));

cl = conv(logical([1 0 1]), [1 1]);
fprintf('conv(logical,double) = [%g %g %g %g] class=%s   expect [1 1 1 1] double\n', cl(1), cl(2), cl(3), cl(4), class(cl));

% Regression: plain double*double unchanged.
cd_ = conv([1 2 3], [4 5 6]);
fprintf('conv(double,double) = [%g %g %g %g %g] class=%s   expect [4 13 28 27 18] double\n', ...
        cd_(1), cd_(2), cd_(3), cd_(4), cd_(5), class(cd_));

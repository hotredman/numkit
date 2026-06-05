clear

import compat.*

% bugs/stats/movfun-typeclass.md — movsum/movprod/movmean on integer/logical.
% MATLAB PROMOTES integer/logical to double for these arithmetic moving
% functions (class NOT preserved). Window 3, endpoints 'shrink'.

s = movsum(int8([3 1 2 5 4]), 3);
fprintf('movsum(int8([3 1 2 5 4]),3) = [%g %g %g %g %g]   expect [4 6 8 11 9], isa double=%d (expect 1)\n', ...
        s(1), s(2), s(3), s(4), s(5), isa(s,'double'));

p = movprod(int8([3 1 2 5 4]), 3);
fprintf('movprod(...) = [%g %g %g %g %g]   expect [3 6 10 40 20]\n', p(1), p(2), p(3), p(4), p(5));

m = movmean(int8([3 1 2 5 4]), 3);
fprintf('movmean(...) = [%.4g %.4g %.4g %.4g %.4g]   expect [2 2 2.667 3.667 4.5]\n', m(1), m(2), m(3), m(4), m(5));

l = movsum(logical([1 0 1 1 0]), 3);
fprintf('movsum(logical([1 0 1 1 0]),3) = [%g %g %g %g %g]   expect [1 2 2 2 1], isa double=%d (expect 1)\n', ...
        l(1), l(2), l(3), l(4), l(5), isa(l,'double'));

u = movsum(uint16([30 10 50 20]), 2);
fprintf('movsum(uint16([30 10 50 20]),2) = [%g %g %g %g]   expect [30 40 60 70]\n', u(1), u(2), u(3), u(4));

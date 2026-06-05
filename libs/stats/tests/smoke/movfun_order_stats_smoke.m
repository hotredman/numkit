clear

import compat.*

% bugs/stats/movfun-order-stats.md — movmax/movmin/movmedian on integer/logical.
% MATLAB PRESERVES the class for these order statistics: movmax/movmin int->int
% & logical->logical; movmedian int->int (round half away), logical->double.

a = movmax(int8([3 1 2 5 4]), 3);
fprintf('movmax(int8,3) = [%d %d %d %d %d] class=%s   expect [3 3 5 5 5] int8\n', a(1),a(2),a(3),a(4),a(5), class(a));

b = movmin(int8([3 1 2 5 4]), 3);
fprintf('movmin(int8,3) = [%d %d %d %d %d]   expect [1 1 1 2 4]\n', b(1),b(2),b(3),b(4),b(5));

m = movmedian(int8([3 1 2 5 4]), 3);
fprintf('movmedian(int8,3) = [%d %d %d %d %d] class=%s   expect [2 2 2 4 5] int8\n', m(1),m(2),m(3),m(4),m(5), class(m));

m2 = movmedian(int8([3 1 2 5 4]), 2);
fprintf('movmedian(int8,2) = [%d %d %d %d %d]   expect [3 2 2 4 5] (1.5->2,3.5->4,4.5->5)\n', m2(1),m2(2),m2(3),m2(4),m2(5));

n = movmedian(int8([-1 -2 -4 -5]), 2);
fprintf('movmedian neg = [%d %d %d %d]   expect [-1 -2 -3 -5] (round half away)\n', n(1),n(2),n(3),n(4));

lx = movmax(logical([1 0 1 1 0]), 3);
fprintf('movmax(logical,3) = [%d %d %d %d %d] class=%s   expect [1 1 1 1 1] logical\n', lx(1),lx(2),lx(3),lx(4),lx(5), class(lx));

lmd = movmedian(logical([1 0 1 1 0]), 3);
fprintf('movmedian(logical,3) = [%.4g %.4g %.4g %.4g %.4g] class=%s   expect [0.5 1 1 1 0.5] double\n', ...
        lmd(1),lmd(2),lmd(3),lmd(4),lmd(5), class(lmd));

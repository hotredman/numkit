clear

% bugs/builtin/accumarray-integer-vals.md — accumarray accepts integer/logical
% vals. MATLAB R2025b output class follows the reducer: sum/prod/mean -> double,
% but max/min PRESERVE the integer class. Previously numkit threw
% "accumarray: vals must be DOUBLE".

s = accumarray([1;2;1], int8([10;20;30]));
fprintf('default sum int8 = [%g %g] class=%s   expect [40 20] double\n', s(1), s(2), class(s));

mx = accumarray([1;2;1], int8([100;100;30]), [], @max);
fprintf('@max int8 = [%g %g] class=%s   expect [100 100] int8 (class preserved)\n', mx(1), mx(2), class(mx));

mn = accumarray([1;2;1], int8([10;20;30]), [], @min);
fprintf('@min int8 = [%g %g] class=%s   expect [10 20] int8 (class preserved)\n', mn(1), mn(2), class(mn));

mxu = accumarray([1;2;1], uint16([100;200;30]), [], @max);
fprintf('@max uint16 = [%g %g] class=%s   expect [100 200] uint16\n', mxu(1), mxu(2), class(mxu));

pd = accumarray([1;2;1], int8([10;20;30]), [], @prod);
fprintf('@prod int8 = [%g %g] class=%s   expect [300 20] double\n', pd(1), pd(2), class(pd));

mu = accumarray([1;2;1], int8([10;20;30]), [], @mean);
fprintf('@mean int8 = [%g %g] class=%s   expect [20 20] double\n', mu(1), mu(2), class(mu));

t2 = accumarray([1 1;2 2], int8([5;7]));
fprintf('2-D subs int8 = [%g %g %g %g] class=%s   expect [5 0 0 7] double\n', t2(1), t2(2), t2(3), t2(4), class(t2));

fv = accumarray([1;3], int8([5;7]), [4 1], @sum, -1);
fprintf('fillval -1 = [%g %g %g %g]   expect [5 -1 7 -1]\n', fv(1), fv(2), fv(3), fv(4));

lg = accumarray([1;2;1], logical([1;0;1]));
fprintf('logical default-sum = [%g %g] class=%s   expect [2 0] double (counts)\n', lg(1), lg(2), class(lg));

% Regression: plain double vals unchanged.
dd = accumarray([1;2;1], [10;20;30]);
fprintf('double vals = [%g %g] class=%s   expect [40 20] double\n', dd(1), dd(2), class(dd));

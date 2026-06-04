clear
import compat.*
% str2func: bare name, '@name', and '@(...)' anonymous source.
f = str2func('sin');
fprintf('str2func(''sin'')(0)        = %g  (expect 0)\n', f(0));
g = str2func('@cos');
fprintf('str2func(''@cos'')(0)       = %g  (expect 1)\n', g(0));
h = str2func('@(x) x + 1');
fprintf('str2func(''@(x)x+1'')(5)    = %g  (expect 6)\n', h(5));
k = str2func('@(x,y) x*y');
fprintf('str2func(''@(x,y)x*y'')(3,4)= %g  (expect 12)\n', k(3, 4));

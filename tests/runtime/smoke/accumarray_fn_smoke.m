clear
import compat.*
% accumarray with an ARBITRARY function handle (named or anonymous) applied to
% each output cell's column of values — not just the built-in reducers.
a = accumarray([1;1;2], [1;2;3], [], @(x) sum(x.^2));
fprintf('sum-of-squares: '); fprintf('%g ', a); fprintf(' (expect 5 9)\n');

b = accumarray([1;2;1;3], [10;20;30;40], [], @(x) max(x) - min(x));
fprintf('range:          '); fprintf('%g ', b); fprintf(' (expect 20 0 0)\n');

c = accumarray([1;3], [10;30], [], @(x) mean(x), -1);
fprintf('mean + fill -1: '); fprintf('%g ', c); fprintf(' (expect 10 -1 30)\n');

% Built-in reducers are unchanged.
fprintf('@max (built-in):'); fprintf(' %g', accumarray([1;2;1], [10;20;30], [], @max));
fprintf(' (expect 30 20)\n');

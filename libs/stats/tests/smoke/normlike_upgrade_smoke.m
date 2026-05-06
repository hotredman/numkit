clear

import compat.*

x = [1.2 2.4 3.1 4.5 5.0 6.2 7.1]';
cens = [0 0 0 0 0 1 1]';
freq = [2 2 2 1 1 1 1]';

fprintf('basic    : %.6f (expect 17.473048)\n', normlike([3, 1.5], x));
fprintf('cens     : %.6f (expect 18.685815)\n', normlike([3, 1.5], x, cens));
fprintf('freq     : %.6f (expect 22.248481)\n', normlike([3, 1.5], x, [], freq));
fprintf('cens+freq: %.6f (expect 23.461248)\n', normlike([3, 1.5], x, cens, freq));
fprintf('empty    : %.6f (expect 0)\n', normlike([3, 1.5], []));
fprintf('sigma=0  : %g (expect NaN)\n', normlike([3, 0], x));
fprintf('NaN data : %g (expect NaN)\n', normlike([3, 1.5], [1 2 NaN 4]'));

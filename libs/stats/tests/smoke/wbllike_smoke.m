clear

import compat.*

data = [1 2 3 4 5]';
cens = [0 0 0 1 1]';
freq = [2 2 1 1 1]';

fprintf('=== wbllike ===\n');
fprintf('  basic    : nL=%.4f (expect 46.7468)\n', wbllike([1, 2], data));
fprintf('  cens     : nL=%.4f (expect 51.1288)\n', wbllike([1, 2], data, cens));
fprintf('  freq     : nL=%.4f (expect 49.6673)\n', wbllike([1, 2], data, [], freq));
fprintf('  combined : nL=%.4f (expect 54.0494)\n', wbllike([1, 2], data, cens, freq));
fprintf('  scale=0  : nL=%g (NaN — was +Inf)\n', wbllike([0, 2], data));
fprintf('  shape<0  : nL=%g (NaN — was +Inf)\n', wbllike([1, -1], data));
fprintf('  x has 0  : nL=%g (NaN — was +Inf)\n', wbllike([1, 2], [0; 2; 3]));
% AVAR (2-output form): not yet implemented; deferred.

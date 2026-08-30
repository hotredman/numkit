clear

data = [1 2 3 4 5]';
cens = [0 0 0 1 1]';
freq = [2 2 1 1 1]';

fprintf('=== evlike ===\n');
fprintf('  basic    : nL=%.4f (expect 218.2042)\n', evlike([0, 1], data));
fprintf('  cens     : nL=%.4f (expect 227.2042)\n', evlike([0, 1], data, cens));
fprintf('  freq     : nL=%.4f (expect 225.3115)\n', evlike([0, 1], data, [], freq));
fprintf('  combined : nL=%.4f (expect 234.3115)\n', evlike([0, 1], data, cens, freq));
fprintf('  σ=0      : nL=%g (NaN — was +Inf)\n', evlike([0, 0], data));
fprintf('  σ<0      : nL=%g (NaN — was +Inf)\n', evlike([0, -1], data));
fprintf('  empty    : nL=%g (0 — was +Inf)\n', evlike([0, 1], []));
% AVAR (2-output form): not yet implemented; deferred.

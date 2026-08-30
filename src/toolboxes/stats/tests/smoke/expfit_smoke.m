clear

x    = [1 2 3 4 5 6 7 8 9 10]';
cens = [0 0 0 0 0 0 0 1 1 1]';
freq = [2 2 2 1 1 1 1 1 1 1]';

fprintf('=== expfit ===\n');
[mu, ci] = expfit(x);
fprintf('  basic:    mu=%.4f  ci=[%.4f, %.4f]\n', mu, ci(1), ci(2));
fprintf('            (expect 5.5000 [3.2192, 11.4694])\n');

[mu, ci] = expfit(x, 0.05, cens);
fprintf('  cens:     mu=%.4f  ci=[%.4f, %.4f]\n', mu, ci(1), ci(2));
fprintf('            (expect 7.8571 [4.2115, 19.5426])\n');

[mu, ci] = expfit(x, 0.05, [], freq);
fprintf('  freq:     mu=%.4f  ci=[%.4f, %.4f]\n', mu, ci(1), ci(2));
fprintf('            (expect 4.6923 [2.9101, 8.8125])\n');

[mu, ci] = expfit(x, 0.05, cens, freq);
fprintf('  combined: mu=%.4f  ci=[%.4f, %.4f]\n', mu, ci(1), ci(2));
fprintf('            (expect 6.1000 [3.5704, 12.7206])\n');

% Edge: all censored -> no events -> NaN.
[mu, ci] = expfit([1 2 3]', 0.05, [1 1 1]');
fprintf('  all-cens: mu=%g (NaN)\n', mu);

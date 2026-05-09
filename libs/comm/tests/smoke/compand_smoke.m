clear
import compat.*

fprintf('=== compand (mu-law / A-law compander) ===\n');

x = [0 0.25 0.5 0.75 1.0];

fprintf('  mu compress (mu=255, V=1):\n    ');
y_mu_c = compand(x, 255, 1, 'mu/compressor');
fprintf('%.6f ', y_mu_c);
fprintf('\n    expect 0.000000 0.752101 0.875703 0.948355 1.000000\n');

fprintf('  mu expand back (round-trip):\n    ');
y_mu_e = compand(y_mu_c, 255, 1, 'mu/expander');
fprintf('%.6f ', y_mu_e);
fprintf('\n    expect 0.00 0.25 0.50 0.75 1.00\n');

fprintf('  A compress (A=87.6, V=1):\n    ');
y_a_c = compand(x, 87.6, 1, 'A/compressor');
fprintf('%.6f ', y_a_c);
fprintf('\n    expect 0.000000 0.746693 0.873346 0.947434 1.000000\n');

fprintf('  A expand back (round-trip):\n    ');
y_a_e = compand(y_a_c, 87.6, 1, 'A/expander');
fprintf('%.6f ', y_a_e);
fprintf('\n    expect 0.00 0.25 0.50 0.75 1.00\n');

fprintf('  mu compress on negatives (sign preserved):\n    ');
y_mu_neg = compand(-x, 255, 1, 'mu/compressor');
fprintf('%.6f ', y_mu_neg);
fprintf('\n    expect 0.000000 -0.752101 -0.875703 -0.948355 -1.000000\n');

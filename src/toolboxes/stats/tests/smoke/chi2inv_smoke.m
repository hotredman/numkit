clear

ps = [0.05 0.5 0.95];

fprintf('=== chi2inv ===\n');

x = chi2inv(ps, 1);
fprintf('  k=1   : [%.4f %.4f %.4f] (expect [0.0039 0.4549 3.8415])\n', x(1), x(2), x(3));

x = chi2inv(ps, 5);
fprintf('  k=5   : [%.4f %.4f %.4f] (expect [1.1455 4.3515 11.0705])\n', x(1), x(2), x(3));

x = chi2inv(ps, 30);
fprintf('  k=30  : [%.4f %.4f %.4f] (expect [18.4927 29.3360 43.7730])\n', x(1), x(2), x(3));

fprintf('\n--- edges ---\n');
fprintf('  p=0  : %g (expect 0)\n', chi2inv(0.0, 5));
fprintf('  p=1  : %g (expect Inf)\n', chi2inv(1.0, 5));
fprintf('  p<0  : %g (expect NaN)\n', chi2inv(-0.1, 5));
fprintf('  p>1  : %g (expect NaN)\n', chi2inv( 1.5, 5));
fprintf('  k=0  : %g (expect 0; degenerate)\n', chi2inv(0.5, 0));
fprintf('  k<0  : %g (expect NaN)\n', chi2inv(0.5, -1));

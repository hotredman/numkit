clear

fprintf('=== chi2pdf ===\n');
fprintf('  scalar (2, 3)    = %.6f (expect 0.207554)\n', chi2pdf(2, 3));
fprintf('  density-at-0 k=2 = %.4f (expect 0.5; Chi2(2) ≡ Exp(1/2))\n', chi2pdf(0, 2));

y = chi2pdf([0.5 1 2 5]', 3);
fprintf('\n  vector x [0.5 1 2 5]'' k=3:\n');
fprintf('    [%.4f %.4f %.4f %.4f]\n', y(1), y(2), y(3), y(4));
fprintf('    expect [0.2197 0.2420 0.2076 0.0732]\n');

fprintf('\n--- edges ---\n');
fprintf('  x<0    : %g (expect 0)\n', chi2pdf(-1, 3));
fprintf('  k=0    : %g (expect 0; degenerate Chi2(0))\n', chi2pdf(2, 0));
fprintf('  k<0    : %g (expect NaN)\n', chi2pdf(2, -1));
fprintf('  large k=30, x=30: %.6f (expect 0.051218)\n', chi2pdf(30, 30));

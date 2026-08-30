clear

% Reference MATLAB R2025b values for nctpdf(x, 10, 2) and nctcdf(x, 10, 2):
fprintf('nctpdf(1.5, 10, 2)         = %.10f  (expect ~0.3349931548)\n', nctpdf(1.5, 10, 2));
fprintf('nctcdf(1.5, 10, 2)         = %.10f  (expect ~0.3047854474)\n', nctcdf(1.5, 10, 2));
fprintf('nctcdf(1.5, 10, 2, upper)  = %.10f  (expect ~0.6952145526)\n', nctcdf(1.5, 10, 2, 'upper'));

% Central limit: δ=0 → tpdf / tcdf.
fprintf('\nCentral-limit check:\n');
fprintf('  nctpdf(0.5, 7, 0) = %.12f\n', nctpdf(0.5, 7, 0));
fprintf('  tpdf  (0.5, 7   ) = %.12f  (should be equal)\n', tpdf(0.5, 7));

% Negative argument symmetry: F(x; ν, δ) = 1 - F(-x; ν, -δ).
a = nctcdf(-1.5, 10, 2);
b = 1 - nctcdf(1.5, 10, -2);
fprintf('\nSymmetry F(-1.5; 10, 2) = %.12f\n', a);
fprintf('         1 - F(1.5; 10, -2) = %.12f  (should match)\n', b);

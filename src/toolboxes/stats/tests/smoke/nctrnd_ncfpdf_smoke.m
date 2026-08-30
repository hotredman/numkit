clear

rng(0);
S = nctrnd(10, 2, 5000, 1);
fprintf('nctrnd(10, 2, 5000, 1):\n');
fprintf('  sample mean = %.4f  (true 2.1674)\n', mean(S));
fprintf('  sample var  = %.4f  (true 1.5522)\n', var(S));

fprintf('\nncfpdf(1.5, 5, 10, 3) = %.12f  (MATLAB: 0.343970191513)\n', ncfpdf(1.5, 5, 10, 3));
y = ncfpdf([0.5 1.0 2.0 3.0], 5, 10, 3);
fprintf('ncfpdf vector: '); disp(y);
fprintf('       expect: [0.3541 0.4227 0.2493 0.1203]\n');

% Central limit check
fprintf('\nncfpdf delta=0 vs fpdf:\n');
fprintf('  ncfpdf(1.5, 5, 10, 0) = %.12f\n', ncfpdf(1.5, 5, 10, 0));
fprintf('  fpdf  (1.5, 5, 10  ) = %.12f  (should match)\n', fpdf(1.5, 5, 10));

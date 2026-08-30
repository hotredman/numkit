clear

% --- Shape and dimensions ---
rng(42);
x = randn(1, 64);
swc = swt(x, 3, 'haar');
fprintf('--- swt(randn(1,64), 3, ''haar'') ---\n');
fprintf('  size(swc) = %dx%d (expect 4x64)\n', size(swc, 1), size(swc, 2));
fprintf('  row 1 = cD_1 (finest), row 4 = cA_3 (coarsest)\n\n');

% --- iswt round-trip on Haar ---
xr = iswt(swc, 'haar');
fprintf('--- iswt round-trip Haar: max err = %.6e (expect ~ 1e-13) ---\n\n', ...
    max(abs(x - xr)));

% --- iswt round-trip on db2 ---
swc = swt(x, 3, 'db2');
xr  = iswt(swc, 'db2');
fprintf('--- iswt round-trip db2: max err = %.6e ---\n\n', max(abs(x - xr)));

% --- iswt round-trip on sym4 (signal must be div by 16 for n=4) ---
x = randn(1, 128);
swc = swt(x, 4, 'sym4');
xr  = iswt(swc, 'sym4');
fprintf('--- iswt round-trip sym4 4-level: max err = %.6e ---\n\n', ...
    max(abs(x - xr)));

% --- Constant signal: all detail rows ≈ 0, approx ≈ const ---
c = 5 * ones(1, 32);
swc = swt(c, 2, 'haar');
fprintf('--- swt on constant: max|details| = %.6e (expect ~0) ---\n', ...
    max(max(abs(swc(1:2, :)))));
fprintf('  approx row mean = %.4f (expect ≈ 5*sqrt(2)*sqrt(2) = ?)\n', ...
    mean(swc(3, :)));
fprintf('  (Haar 2-level on const propagates a coherent gain)\n\n');

% --- Length constraint ---
try
    swt(randn(1, 30), 3, 'haar');   % 30 not divisible by 8
    fprintf('--- length-constraint error NOT raised (BUG)\n');
catch err
    fprintf('--- length-constraint correctly raised: ''%s''\n', err.message);
end

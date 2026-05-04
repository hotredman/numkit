import compat.*

% --- wfilters round-trip QMF properties ---
[Lo_D, Hi_D, Lo_R, Hi_R] = wfilters('haar');
fprintf('--- wfilters(''haar'') ---\n');
disp([Lo_D; Hi_D; Lo_R; Hi_R]);
fprintf('  expect rows: [1/sqrt2 1/sqrt2; -1/sqrt2 1/sqrt2; 1/sqrt2 1/sqrt2; 1/sqrt2 -1/sqrt2]\n\n');

[Lo_D, Hi_D, Lo_R, Hi_R] = wfilters('db2');
fprintf('--- wfilters(''db2'') sums ---\n');
fprintf('  sum(Lo_D) = %.6f (expect ~ sqrt(2) = 1.4142)\n', sum(Lo_D));
fprintf('  sum(Hi_D) = %.6f (expect ~ 0)\n', sum(Hi_D));
fprintf('  ||Lo_R||  = %.6f (expect ~ 1)\n', sqrt(sum(Lo_R.^2)));
fprintf('  ||Hi_R||  = %.6f (expect ~ 1)\n\n', sqrt(sum(Hi_R.^2)));

% --- DWT/IDWT round-trip on a constant signal ---
x = ones(1, 16);
[cA, cD] = dwt(x, 'haar');
fprintf('--- dwt(ones(1,16), haar) ---\n');
fprintf('  cA = '); disp(cA);
fprintf('  cD = '); disp(cD);
fprintf('  expect cA = [sqrt2 ... sqrt2] (8 entries), cD ≈ 0\n\n');

xr = idwt(cA, cD, 'haar');
fprintf('--- idwt round-trip ---\n');
fprintf('xr = '); disp(xr);
fprintf('max|x - xr| = %.6e (expect ~ 0)\n\n', max(abs(x - xr)));

% --- Round-trip on random with db2 ---
rng(7);
x = randn(1, 32);
[cA, cD] = dwt(x, 'db2');
xr = idwt(cA, cD, 'db2', 32);
fprintf('--- idwt(dwt(x, db2)) round-trip on randn(1,32) ---\n');
fprintf('  length(cA) = %d, length(cD) = %d (expect 17 each)\n', numel(cA), numel(cD));
fprintf('  max|x - xr| = %.6e (expect ~ 1e-13)\n\n', max(abs(x - xr)));

% --- Round-trip with sym4 ---
x = randn(1, 64);
[cA, cD] = dwt(x, 'sym4');
xr = idwt(cA, cD, 'sym4', 64);
fprintf('--- sym4 round-trip on randn(1,64): max err = %.6e ---\n', ...
    max(abs(x - xr)));

% --- Round-trip with coif1 ---
x = randn(1, 64);
[cA, cD] = dwt(x, 'coif1');
xr = idwt(cA, cD, 'coif1', 64);
fprintf('--- coif1 round-trip: max err = %.6e ---\n', max(abs(x - xr)));

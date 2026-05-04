clear

import compat.*

% --- 3-level wavedec/waverec round-trip on ramp ---
x = 1:64;
[C, L] = wavedec(x, 3, 'db2');
fprintf('--- wavedec(1:64, 3, db2) ---\n');
fprintf('  L = '); disp(L);
fprintf('  expect L of length 5: [|cA3| |cD3| |cD2| |cD1| 64]\n');
fprintf('  numel(C) = sum(L(1:end-1)) = %d (expect %d)\n\n', numel(C), sum(L(1:end-1)));

xr = waverec(C, L, 'db2');
fprintf('--- waverec(C, L, db2) ---\n');
fprintf('  numel(xr) = %d (expect 64)\n', numel(xr));
fprintf('  max|x - xr| = %.6e (expect ~ 1e-12)\n\n', max(abs(x - xr)));

% --- detcoef on each level ---
fprintf('--- detcoef ---\n');
for k = 1:3
    d = detcoef(C, L, k);
    fprintf('  detcoef level %d: length = %d\n', k, numel(d));
end
fprintf('\n');

% --- appcoef at deepest level vs explicit cA from C ---
cA3 = appcoef(C, L, 'db2', 3);
fprintf('--- appcoef level 3 ---\n');
fprintf('  numel = %d (expect %d)\n', numel(cA3), L(1));
fprintf('  first = %.4f (expect = C(1) = %.4f)\n\n', cA3(1), C(1));

% --- appcoef at level 0 = full reconstruction ---
xr2 = appcoef(C, L, 'db2', 0);
fprintf('--- appcoef level 0 (full reconstruction) ---\n');
fprintf('  max|x - xr2| = %.6e (expect ~ 1e-12)\n\n', max(abs(x - xr2)));

% --- Round-trip with sym4 on randn ---
rng(11);
x = randn(1, 128);
[C, L] = wavedec(x, 4, 'sym4');
xr = waverec(C, L, 'sym4');
fprintf('--- sym4 4-level round-trip on randn(1,128): max err = %.6e ---\n', ...
    max(abs(x - xr)));

% --- 1-level wavedec must match dwt directly ---
x = randn(1, 32);
[cA, cD] = dwt(x, 'haar');
[C, L] = wavedec(x, 1, 'haar');
fprintf('--- 1-level wavedec matches dwt? max err = %.6e ---\n', ...
    max(abs(C - [cA cD])));

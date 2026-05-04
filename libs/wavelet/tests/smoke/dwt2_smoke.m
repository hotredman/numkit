import compat.*

% --- Shape and band semantics ---
rng(7);
X = round(100 * rand(8, 8));
[cA, cH, cV, cD] = dwt2(X, 'haar');
fprintf('--- dwt2(8x8 random, haar) ---\n');
fprintf('  size(cA) = %dx%d (expect 4x4 for haar)\n', ...
    size(cA, 1), size(cA, 2));
fprintf('  size(cH) = %dx%d, size(cV) = %dx%d, size(cD) = %dx%d\n\n', ...
    size(cH,1), size(cH,2), size(cV,1), size(cV,2), size(cD,1), size(cD,2));

% --- Round-trip on Haar ---
Xr = idwt2(cA, cH, cV, cD, 'haar');
fprintf('--- idwt2 round-trip Haar ---\n');
fprintf('  size(Xr) = %dx%d, max|X - Xr| = %.6e\n\n', ...
    size(Xr,1), size(Xr,2), max(max(abs(X - Xr))));

% --- Round-trip on db2 ---
[cA, cH, cV, cD] = dwt2(X, 'db2');
fprintf('--- size dwt2 db2 cA = %dx%d ---\n', size(cA,1), size(cA,2));
Xr = idwt2(cA, cH, cV, cD, 'db2', size(X));
fprintf('--- idwt2 round-trip db2: max err = %.6e ---\n\n', max(max(abs(X - Xr))));

% --- Non-square round-trip ---
rng(42);
X = randn(12, 16);
[cA, cH, cV, cD] = dwt2(X, 'sym4');
Xr = idwt2(cA, cH, cV, cD, 'sym4', [12 16]);
fprintf('--- non-square 12x16 sym4 round-trip: max err = %.6e ---\n\n', ...
    max(max(abs(X - Xr))));

% --- Constant image: only cA non-zero ---
C = 5 * ones(4, 4);
[cA, cH, cV, cD] = dwt2(C, 'haar');
fprintf('--- dwt2 of 5*ones(4,4) ---\n');
fprintf('  cA(1,1) = %.4f (expect 10 = 5*sqrt(2)*sqrt(2))\n', cA(1,1));
fprintf('  max|cH| = %.6e, max|cV| = %.6e, max|cD| = %.6e (expect ~ 0)\n', ...
    max(max(abs(cH))), max(max(abs(cV))), max(max(abs(cD))));

% --- Edge sensitivity: vertical edge → cV non-trivial ---
% Use sym4 so the longer filter actually catches the edge regardless
% of phase; Haar's 2-tap filter would miss an edge at even index.
E = zeros(8, 8);
E(:, 1:4) = 1;
[cA, cH, cV, cD] = dwt2(E, 'sym4');
fprintf('\n--- dwt2 of vertical-step image (sym4) ---\n');
fprintf('  ||cH||_F = %.4f, ||cV||_F = %.4f (expect cV >> cH for vertical edge)\n', ...
    sqrt(sum(sum(cH.^2))), sqrt(sum(sum(cV.^2))));

clear

% partialcorr smoke covering all 3 documented forms.
% Reference: MATLAB R2025b.

fprintf('=== partialcorr(X, Y, Z) — 3-arg form ===\n');
x = [1; 2; 3; 4; 5; 6; 7; 8; 9; 10];
y = [1.2; 1.8; 3.5; 3.9; 5.1; 6.0; 7.2; 7.8; 9.1; 10.0];
z = [1; 4; 2; 5; 3; 6; 8; 7; 9; 10];

cn = corr([x y]);
fprintf('  naive corr(x, y) = %g (high — both correlate with z)\n', cn(1, 2));
fprintf('  partialcorr(x, y, z) = %g (residual after regressing out z)\n', ...
        partialcorr(x, y, z));

% Multi-column X and Y.
X2 = [x x.^2];
Y2 = [y y.^2 y.^3];
P = partialcorr(X2, Y2, z);
fprintf('\n  partialcorr(2-col X, 3-col Y, z) -> 2x3 matrix:\n');
disp(P);

fprintf('\n=== partialcorr(X) — 1-arg form (cycle 85) ===\n');
% Control = the remaining X columns; per-pair distinct.
X = [1 2 3; 4 1 5; 7 5 2; 2 8 6; 9 3 7; 5 6 4; 3 9 8; 8 4 1];
R1 = partialcorr(X);
fprintf('  partialcorr(X) 3x3:\n');
disp(R1);
fprintf('  (e R(1,2)=-0.13785  R(1,3)=-0.16363  R(2,3)= 0.36197)\n');

% 2-column X reduces to corr(X) — control set is empty (only intercept).
X2c = [1 2; 3 5; 5 4; 7 8; 9 7];
Pp = partialcorr(X2c);
Pc = corr(X2c);
fprintf('\n  partialcorr(X2) == corr(X2) when X has only 2 cols:\n');
fprintf('    partialcorr off-diag = %.6f, corr off-diag = %.6f\n', ...
        Pp(1, 2), Pc(1, 2));

fprintf('\n=== partialcorr(X, Z) — 2-arg form (cycle 85) ===\n');
% Control = Z; same for every pair.
Z = [1 -1; 2 -2; 3 -1; 1 -3; 2 -2; 4 -1; 3 -3; 1 -1];
R2 = partialcorr(X, Z);
fprintf('  partialcorr(X, Z) 3x3:\n');
disp(R2);
fprintf('  (e R(1,2)=-0.11772  R(1,3)= 0.06451  R(2,3)=-0.54406)\n');

% ── 'Rows' NaN policy (2026-05-30): 'complete' listwise deletion ──
% partialcorr previously ignored 'Rows' (NaN-poisoned NaN data). 'complete'
% drops every row with a NaN before computing the partial correlation;
% 'all' (default) still NaN-poisons; 'pairwise' deferred. vs MATLAB R2025b.
fprintf('\n=== Rows complete (listwise) ===\n');
Xn = [1 5 2; 2 6 9; 3 NaN 4; 4 8 1; 5 9 6; 6 3 NaN; 7 2 5];
fprintf('partialcorr(Xn,''Rows'',''complete'') ->\n');
disp(partialcorr(Xn,'Rows','complete'));
fprintf('(expect [1 -0.22712 0.040291; -0.22712 1 -0.046205; 0.040291 -0.046205 1])\n');
fprintf('partialcorr(Xn) default -> NaN-poison: %s\n', mat2str(partialcorr(Xn),4));

clear

% corrcoef 'Rows' NaN policy (2026-05-30): corrcoef(X, ..., 'Rows', R).
% 'all' (default) NaN-poisons; 'complete' drops every row with a NaN
% (listwise); 'pairwise' deletes pairwise, normalizing each (i,j) over
% its own common non-NaN rows. corrcoef previously errored/NaN-poisoned
% with 'Rows'. 'pairwise' is Pearson-only; [R,P] with 'pairwise' deferred.
% vs MATLAB R2025b.

Xn = [1 5 2; 2 6 9; 3 NaN 4; 4 8 1; 5 9 NaN];

fprintf('=== all (default, NaN-poison) ===\n');
fprintf('corrcoef(Xn) -> %s\n', mat2str(corrcoef(Xn),5));

fprintf('\n=== complete (listwise) ===\n');
fprintf('corrcoef(Xn,''Rows'',''complete'') -> %s\n', mat2str(corrcoef(Xn,'Rows','complete'),5));
fprintf('  (expect [1 1 -0.30038; 1 1 -0.30038; -0.30038 -0.30038 1])\n');

fprintf('\n=== pairwise ===\n');
fprintf('corrcoef(Xn,''Rows'',''pairwise'') -> %s\n', mat2str(corrcoef(Xn,'Rows','pairwise'),5));
fprintf('  (expect (1,3)=-0.29019 differs from (2,3)=-0.30038)\n');

fprintf('\n=== [R,P] complete ===\n');
[R,P] = corrcoef(Xn,'Rows','complete');
fprintf('P(1,3) = %.6f (expect 0.805776)\n', P(1,3));

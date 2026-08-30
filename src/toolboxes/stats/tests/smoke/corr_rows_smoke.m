clear

% corr 'Rows' NaN policy (2026-05-30): corr(X, ..., 'Rows', R).
% 'all' (default) NaN-poisons; 'complete' drops every row with a NaN
% (listwise); 'pairwise' deletes pairwise (each (i,j) uses rows where
% both columns are non-NaN). corr previously ignored 'Rows'. 'pairwise'
% is Pearson-only for now. vs MATLAB R2025b.

Xn = [1 5 2; 2 6 9; 3 NaN 4; 4 8 1; 5 9 NaN];

fprintf('=== all (default, NaN-poison) ===\n');
fprintf('corr(Xn)               -> %s\n', mat2str(corr(Xn),5));

fprintf('\n=== complete (listwise) ===\n');
fprintf('corr(Xn,''rows'',''complete'') -> %s\n', mat2str(corr(Xn,'rows','complete'),5));
fprintf('  (expect [1 1 -0.30038; 1 1 -0.30038; -0.30038 -0.30038 1])\n');

fprintf('\n=== pairwise ===\n');
fprintf('corr(Xn,''rows'',''pairwise'') -> %s\n', mat2str(corr(Xn,'rows','pairwise'),5));
fprintf('  (expect (1,3)=-0.29019 differs from (2,3)=-0.30038)\n');

fprintf('\n=== two-vector complete ===\n');
fprintf('corr(x,y,''rows'',''complete'') -> %s (expect 1)\n', mat2str(corr([1;2;3;NaN;5],[2;4;6;8;NaN],'rows','complete'),5));

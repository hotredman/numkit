clear

% cov NaN-policy flag (2026-05-30): cov(X, ..., 'omitrows'|'partialrows').
% 'omitrows' drops every row containing a NaN (listwise deletion);
% 'partialrows' deletes pairwise (each cov(i,j) uses rows where both
% columns are non-NaN). 'includenan' (default) NaN-poisons. Previously
% cov(X,'omitrows') errored. vs MATLAB R2025b.

X = [1 5; 2 6; 3 NaN; 4 8];

fprintf('=== includenan (default) ===\n');
fprintf('cov(X)            -> %s (expect [1.6667 NaN; NaN NaN])\n', mat2str(cov(X),5));

fprintf('\n=== omitrows (drop row 3) ===\n');
fprintf('cov(X,''omitrows'') -> %s (expect [2.3333 2.3333; 2.3333 2.3333])\n', mat2str(cov(X,'omitrows'),5));
fprintf('cov(X,1,''omitrows'')-> %s (N-normalized)\n', mat2str(cov(X,1,'omitrows'),5));

fprintf('\n=== partialrows (pairwise) ===\n');
fprintf('cov(X,''partialrows'') -> %s (expect [1.6667 2.3333; 2.3333 2.3333])\n', mat2str(cov(X,'partialrows'),5));

fprintf('\n=== vector + two-vector ===\n');
fprintf('cov([1 2 NaN 4],''omitrows'') -> %s (expect 2.3333)\n', mat2str(cov([1 2 NaN 4],'omitrows'),5));
fprintf('cov(x,y,''omitrows'')         -> %s\n', mat2str(cov([1 2 3 4],[5 6 NaN 8],'omitrows'),5));

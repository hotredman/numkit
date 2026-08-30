clear

% skewness / kurtosis NaN omission (2026-05-30): MATLAB treats NaN as
% missing and removes it per column before computing the moment.
% numkit previously NaN-poisoned. vs MATLAB R2025b.

Mn = [1 5; 2 NaN; 3 7; 4 100];

fprintf('=== skewness/kurtosis omit NaN per column ===\n');
fprintf('skewness(Mn) -> %s (expect [0 0.70603])\n', mat2str(skewness(Mn),5));
fprintf('kurtosis(Mn) -> %s (expect [1.64 1.5])\n',  mat2str(kurtosis(Mn),5));

fprintf('\n=== flag 0 (bias-corrected) ===\n');
fprintf('skewness(Mn,0) -> %s\n', mat2str(skewness(Mn,0),5));
fprintf('kurtosis(Mn,0) -> %s (col2 NaN: <4 non-NaN values)\n', mat2str(kurtosis(Mn,0),5));

fprintf('\n=== clean data unchanged ===\n');
fprintf('skewness([1 5;2 6;3 7;4 100]) -> %s (expect [0 1.1537])\n', mat2str(skewness([1 5;2 6;3 7;4 100]),5));

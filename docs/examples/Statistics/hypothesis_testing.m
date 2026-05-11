% hypothesis_testing.m
% Compare three popular two-sample tests:
%   • ttest2  — assumes normal data, equal variances
%   • ranksum — Mann-Whitney U, distribution-free
%   • kstest2 — two-sample Kolmogorov-Smirnov
% Generate two samples that differ in mean and see all three reject.

import compat.*;

clear
close all

rng(0);
A = randn(80, 1) + 0.0;            % N(0, 1), n=80
B = randn(80, 1) + 0.6;            % N(0.6, 1), n=80

[h_t, p_t]  = ttest2(A, B);
[p_r, h_r]  = ranksum(A, B);
[h_k, p_k]  = kstest2(A, B);

fprintf('Sample sizes:  |A|=%d  |B|=%d\n', numel(A), numel(B));
fprintf('Sample means:  mean(A)=%.3f  mean(B)=%.3f\n\n', mean(A), mean(B));
fprintf('  ttest2  : reject H0 = %d   p = %.4g\n', h_t, p_t);
fprintf('  ranksum : reject H0 = %d   p = %.4g\n', h_r, p_r);
fprintf('  kstest2 : reject H0 = %d   p = %.4g\n', h_k, p_k);
fprintf('\nAll three tests should reject H0 (means truly differ).\n');

% Visualise distributions overlap (histcounts + bar).
edges = linspace(-4, 4, 25);
centres = 0.5 * (edges(1:end-1) + edges(2:end));
nA = histcounts(A, edges);
nB = histcounts(B, edges);

figure;
bar(centres, nA, 'FaceColor', 'b', 'FaceAlpha', 0.5);
hold on;
bar(centres, nB, 'FaceColor', 'r', 'FaceAlpha', 0.5);
hold off;
legend('A ~ N(0,1)', 'B ~ N(0.6,1)');
xlabel('value'); ylabel('count');
title('Two-sample distributions — visually overlapping, statistically distinct');
grid on;

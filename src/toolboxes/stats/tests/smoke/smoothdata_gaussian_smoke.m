clear

% smoothdata 'gaussian' kernel (DEEP-PROBE 2026-05-31). numkit used
% sigma = (k-1)/4 with a kernel mis-aligned at the truncated edges, so the
% result was wrong at the boundaries AND interior. MATLAB R2025b uses
% sigma = windowLength/5, centred on the CURRENT sample, with 'shrink'
% endpoints (truncate the kernel at the array edges + renormalise).
% vs MATLAB R2025b.

fprintf('=== window 3 ===\n');
g = smoothdata([1 5 2 8 3], 'gaussian', 3);
fprintf('g = [%g %g %g %g %g]\n', g(1), g(2), g(3), g(4), g(5));
fprintf('  expect [1.79834 3.83535 3.49741 6.16984 3.99793]\n');

fprintf('\n=== window 5 ===\n');
h = smoothdata([1 5 2 8 3 9 4], 'gaussian', 5);
fprintf('h = [%g %g %g %g %g %g %g]\n', h(1),h(2),h(3),h(4),h(5),h(6),h(7));
fprintf('  expect [2.47053 3.36497 4.19781 5.20481 5.68621 6.10135 5.66334]\n');

fprintf('\n=== matrix (per column) ===\n');
M = smoothdata([1 5; 2 8; 3 9], 'gaussian', 3);
fprintf('col1 = [%g %g %g]  (expect [1.19959 2 2.80041])\n', M(1,1), M(2,1), M(3,1));

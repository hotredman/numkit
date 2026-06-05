clear

import compat.*

% Math primitives + reductions batch — spec closure 2026-05-09.
% cospi/sinpi + deg2rad/rad2deg + eps + cumsum/cumprod/diff +
% diag + prod/sum.

fprintf('cospi(0.5)         = %g  (expect 0)\n',     cospi(0.5));
fprintf('sinpi(1)           = %g  (expect 0)\n',     sinpi(1));
fprintf('deg2rad(180)       = %.15f  (expect pi)\n', deg2rad(180));
fprintf('rad2deg(pi)        = %.15f  (expect 180)\n', rad2deg(pi));
fprintf('eps(1)             = %g\n',                  eps(1));
disp('cumsum([1..5]):'); disp(cumsum([1 2 3 4 5]));
disp('cumprod([1..5]):'); disp(cumprod([1 2 3 4 5]));
disp('diff([1 4 9 16 25]):'); disp(diff([1 4 9 16 25]));
disp('diag([1 2 3]):'); disp(diag([1 2 3]));
fprintf('sum([1..5]) = %g, prod([1..5]) = %g\n', sum(1:5), prod(1:5));

% Empty-input identities (MATLAB R2025b): default reduction of the 0x0 []
% returns the scalar identity, NOT a 1x0 empty.
fprintf('\n=== empty [] reductions -> scalar identity ===\n');
fprintf('sum([])   = %g (expect 0,   numel %d expect 1)\n', sum([]),  numel(sum([])));
fprintf('prod([])  = %g (expect 1,   numel %d expect 1)\n', prod([]), numel(prod([])));
fprintf('mean([])  isnan=%d (expect 1, numel %d expect 1)\n', isnan(mean([])), numel(mean([])));
disp('sum(zeros(0,3)):');  disp(sum(zeros(0,3)));  fprintf('  expect [0 0 0]\n');
disp('prod(zeros(0,3)):'); disp(prod(zeros(0,3))); fprintf('  expect [1 1 1]\n');
fprintf('numel(sum(zeros(3,0))) = %d (expect 0, stays 1x0)\n', numel(sum(zeros(3,0))));

% max/min of an empty array -> empty (never errors), MATLAB R2025b shape.
fprintf('\n=== max/min of empty -> empty (no error) ===\n');
fprintf('max([])         size %dx%d (expect 0x0)\n', size(max([]),1), size(max([]),2));
fprintf('min([])         size %dx%d (expect 0x0)\n', size(min([]),1), size(min([]),2));
fprintf('max(zeros(0,3)) size %dx%d (expect 0x3)\n', size(max(zeros(0,3)),1), size(max(zeros(0,3)),2));
fprintf('max(zeros(3,0)) size %dx%d (expect 1x0)\n', size(max(zeros(3,0)),1), size(max(zeros(3,0)),2));
[mv, iv] = max([3 1 5 2]);
fprintf('max([3 1 5 2]) = %g idx %g (expect 5 idx 3)\n', mv, iv);

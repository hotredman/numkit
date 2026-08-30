clear

% cceps non-power-of-two phase + 2nd output nd (bugs/signal/cceps-nd-phase).
% Pre-fix the phase-dependent samples past DC were garbage and nd was missing.
% Now: full unwrap + MATLAB rcunwrap linear-phase removal; matches MATLAB R2025b.

y = cceps([1 2 3 4 3 2 1]);          % n = 7 (non-power-of-two)
fprintf('cceps([1 2 3 4 3 2 1]):\n');
fprintf('  numkit: %.4f %.4f %.4f %.4f %.4f %.4f %.4f\n', y(1),y(2),y(3),y(4),y(5),y(6),y(7));
fprintf('  MATLAB: 0.3961 0.5236 0.3835 -0.2409 -0.1640 0.6519 1.2225\n');

[xh1, nd1] = cceps((1:8)');
fprintf('[~,nd] = cceps((1:8)'')         -> nd = %g   (expect 1)\n', nd1);
[xh2, nd2] = cceps([1 2 3 4 3 2 1]);
fprintf('[~,nd] = cceps([1 2 3 4 3 2 1]) -> nd = %g  (expect -3)\n', nd2);

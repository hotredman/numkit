clear
import compat.*

% rceps — real cepstrum + minimum-phase reconstruction.
% DEEP-PROBE 2026-06: non-power-of-two lengths used to return garbage
% (nextPow2 padding made log|X| blow up at the padded near-zero bins), and
% the 2nd output (minimum-phase signal) was missing. Both fixed by
% transforming at the exact signal length. Reference: MATLAB R2025b.

% Odd length (n=7) — previously [-258, 87, ...]; now correct.
[y, ym] = rceps([1 2 3 4 3 2 1]);
fprintf('rceps y:  '); fprintf('%.6f ', y); fprintf('\n');
% expect [0.396084 0.873038 0.517671 -0.202457 -0.202457 0.517671 0.873038]
fprintf('rceps ym: '); fprintf('%.6f ', ym); fprintf('\n');
% expect [1.603952 2.571895 3.739440 3.372840 2.652253 1.415667 0.643953]

% Power-of-two length unchanged.
r8 = rceps((1:8)');
fprintf('rceps(1:8) first = %.6f  (expect 2.007521)\n', r8(1));

% cceps at the exact length is sane (no padding blow-up).
c7 = cceps([1 2 3 4 3 2 1]);
fprintf('cceps max|.| = %.4f  (expect < 10, was ~258 with padding)\n', max(abs(c7)));

clear

% corrcoef 3rd/4th outputs [R,P,RL,RU] — DEEP-PROBE 2026-05-31.
% RL/RU are the lower/upper confidence bounds for each correlation
% coefficient (95% by default, or via the 'Alpha' name-value). These
% were unimplemented (asking for a 3rd output threw "Index exceeds
% array dimensions"). Fisher z-transform: z=atanh(r), se=1/sqrt(n-3),
% zc=norminv(1-alpha/2); RL=tanh(z-zc*se), RU=tanh(z+zc*se).
% Reference: MATLAB R2025b.

x = [1 2 4 3 5 7 6 8]';
y = [2 1 3 5 4 6 8 7]';

[R, P, RL, RU] = corrcoef(x, y);
fprintf('=== default (Alpha 0.05) ===\n');
fprintf('R(1,2)  = %.10f\n', R(1,2));
fprintf('RL(1,2) = %.10f   (expect 0.3116980572)\n', RL(1,2));
fprintf('RU(1,2) = %.10f   (expect 0.9689892089)\n', RU(1,2));
fprintf('diagonal RL(1,1)=%g RU(2,2)=%g (expect 1 1)\n', RL(1,1), RU(2,2));

[Ra, Pa, RLa, RUa] = corrcoef(x, y, 'Alpha', 0.10);
fprintf('\n=== Alpha 0.10 (tighter interval) ===\n');
fprintf('RL(1,2) = %.10f   (expect 0.4328079594)\n', RLa(1,2));
fprintf('RU(1,2) = %.10f   (expect 0.9590994664)\n', RUa(1,2));

fprintf('\n=== 2-output form still works ===\n');
[R2, P2] = corrcoef(x, y);
fprintf('P(1,2) = %.7f\n', P2(1,2));

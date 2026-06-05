clear;
import compat.*;

% lognfit — lognormal MLE with optional censoring + frequency weights.
%
%
% New 2026-05-08:
%   - censored MLE: EM iteration on log(x) until convergence
%   - freq weighting: closed-form weighted moments (no censoring) or
%     fold into EM (with censoring)
%   - CIs: chi²/t for the basic+freq case; analytic Fisher info +
%     Wald (z·SE) with log-σ transform when censored

x    = [1 2 3 4 5 6 7 8 9 10]';
cens = [0 0 0 0 0 0 0 1 1 1]';
freq = [2 2 2 1 1 1 1 1 1 1]';

fprintf('--- basic [p, c] = lognfit(x) ---\n');
[p, c] = lognfit(x);
fprintf('parm = '); disp(p);
fprintf('pci  =\n'); disp(c);
fprintf('expect parm=[1.5104 0.7330] pci=[0.9861 0.5042; 2.0348 1.3382]\n\n');

fprintf('--- censored: lognfit(x, 0.05, cens) ---\n');
[p, c] = lognfit(x, 0.05, cens);
fprintf('parm = '); disp(p);
fprintf('pci  =\n'); disp(c);
fprintf('expect parm=[1.6856 0.9278] pci=[1.0725 0.5288; 2.2988 1.6277]\n\n');

fprintf('--- freq only: lognfit(x, 0.05, [], freq) ---\n');
[p, c] = lognfit(x, 0.05, [], freq);
fprintf('parm = '); disp(p);
fprintf('pci  =\n'); disp(c);
fprintf('expect parm=[1.2997 0.7841] pci=[0.8259 0.5623; 1.7735 1.2943]\n\n');

fprintf('--- combined cens+freq ---\n');
[p, c] = lognfit(x, 0.05, cens, freq);
fprintf('parm = '); disp(p);
fprintf('pci  =\n'); disp(c);
fprintf('expect parm=[1.4215 0.9373] pci=[0.8925 0.5887; 1.9504 1.4923]\n');

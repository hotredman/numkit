clear;
import compat.*;

% normfit — normal MLE with optional censoring + frequency weights.
% Closes audit/findings/stats/normfit.md.
%
% New 2026-05-08:
%   - censored MLE: EM iteration on x (truncated-normal moments)
%   - freq weighting: closed-form weighted moments (no censoring) or
%     fold into EM
%   - CIs: t/chi² for the basic+freq case; analytic Fisher info Wald
%     CI with log-σ transform when censored
%   - Shares the `normal_fit_mle` helper with lognfit (DRY)

x    = [1.2 2.4 3.1 4.5 5.0 6.2 7.1]';
cens = [0 0 0 0 0 1 1]';
freq = [2 2 2 1 1 1 1]';

fprintf('--- basic [mu, sd, muci, sdci] = normfit(x) ---\n');
[mu, sd, muci, sdci] = normfit(x);
fprintf('mu=%.6f sd=%.6f\n', mu, sd);
fprintf('muci=[%.4f; %.4f] sdci=[%.4f; %.4f]\n', muci(1), muci(2), sdci(1), sdci(2));
fprintf('expect mu=4.2143 sd=2.1051 muci=[2.2674; 6.1612] sdci=[1.3565; 4.6356]\n\n');

fprintf('--- censored: normfit(x, 0.05, cens) ---\n');
[mu, sd, muci, sdci] = normfit(x, 0.05, cens);
fprintf('mu=%.6f sd=%.6f\n', mu, sd);
fprintf('muci=[%.4f; %.4f] sdci=[%.4f; %.4f]\n', muci(1), muci(2), sdci(1), sdci(2));
fprintf('expect mu=4.6418 sd=2.5994 muci=[2.6101; 6.6735] sdci=[1.3275; 5.0901]\n\n');

fprintf('--- freq only: normfit(x, 0.05, [], freq) ---\n');
[mu, sd, muci, sdci] = normfit(x, 0.05, [], freq);
fprintf('mu=%.6f sd=%.6f\n', mu, sd);
fprintf('muci=[%.4f; %.4f] sdci=[%.4f; %.4f]\n', muci(1), muci(2), sdci(1), sdci(2));
fprintf('expect mu=3.6200 sd=2.0187 muci=[2.1759; 5.0641] sdci=[1.3885; 3.6853]\n\n');

fprintf('--- combined cens+freq ---\n');
[mu, sd, muci, sdci] = normfit(x, 0.05, cens, freq);
fprintf('mu=%.6f sd=%.6f\n', mu, sd);
fprintf('muci=[%.4f; %.4f] sdci=[%.4f; %.4f]\n', muci(1), muci(2), sdci(1), sdci(2));
fprintf('expect mu=3.8482 sd=2.3324 muci=[2.3656; 5.3308] sdci=[1.3845; 3.9290]\n');

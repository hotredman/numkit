clear
import compat.*

fprintf('=== mle (closed-form max-likelihood) ===\n');

% Normal default (deterministic data)
x = [1.2 0.8 1.5 0.9 1.1 1.3 1.0 0.7 1.4 1.0];
mn = mle(x);
fprintf('  mle([data]) [normal default]: muhat=%g sigmahat=%g\n', mn(1), mn(2));

% Exponential
xe = [0.5 1.0 1.5 2.0 2.5 0.3 0.8 1.2 1.7 0.6];
me = mle(xe, 'distribution', 'exponential');
fprintf('  mle(xe, ''exponential''): muhat=%g\n', me(1));

% Poisson
xp = [0 1 2 3 4 5 1 2 3 2];
mp = mle(xp, 'distribution', 'poisson');
fprintf('  mle(xp, ''poisson''): lambdahat=%g\n', mp(1));

% Lognormal
xl = exp([0.1 0.2 0.3 0.4 0.5]);
ml = mle(xl, 'distribution', 'lognormal');
fprintf('  mle(xl, ''lognormal''): muhat=%g sigmahat=%g\n', ml(1), ml(2));

fprintf('\n  Custom pdf/logpdf/nloglf: NOT YET implemented (Nelder-Mead deferred)\n');

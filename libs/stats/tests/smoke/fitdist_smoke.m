clear
import compat.*

fprintf('=== fitdist (probability-distribution struct) ===\n');

x = [1.2 0.8 1.5 0.9 1.1 1.3 1.0 0.7 1.4 1.0]';
pd = fitdist(x, 'Normal');
fprintf('  fitdist(x, ''Normal''):\n');
fprintf('    DistributionName = %s\n', pd.DistributionName);
fprintf('    ParameterValues  = [%g %g]\n', pd.ParameterValues(1), pd.ParameterValues(2));
fprintf('    ParameterNames   = {%s, %s}\n', pd.ParameterNames{1}, pd.ParameterNames{2});
fprintf('    NumObservations  = %g\n', pd.NumObservations);

xe = [0.5 1.0 1.5 2.0 2.5 0.3 0.8 1.2 1.7 0.6]';
pe = fitdist(xe, 'Exponential');
fprintf('\n  fitdist(xe, ''Exponential''): mu = %g\n', pe.ParameterValues(1));

xp = [0 1 2 3 4 5 1 2 3 2]';
pp = fitdist(xp, 'Poisson');
fprintf('  fitdist(xp, ''Poisson''): lambda = %g\n', pp.ParameterValues(1));

xl = exp([0.1 0.2 0.3 0.4 0.5]');
pl = fitdist(xl, 'Lognormal');
fprintf('  fitdist(xl, ''Lognormal''): mu = %g, sigma = %g\n', ...
        pl.ParameterValues(1), pl.ParameterValues(2));

fprintf('\n  NB: numkit returns a struct (no class methods); MATLAB returns a\n');
fprintf('  probability-distribution OBJECT with .pdf/.cdf/.icdf methods.\n');
fprintf('  Methods are deferred -- use existing pdf/cdf/inv functions instead.\n');

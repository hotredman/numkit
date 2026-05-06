clear

import compat.*

% MATLAB R2025b 'upper' flag — survival functions for the CDF family.
% Closes 14 audit ТЗ in stats.dist.

fprintf('=== continuous distributions ===\n');
fprintf('  normcdf(1.96, 0, 1, "upper")  = %.8f (expect 0.02499790)\n', normcdf(1.96, 0, 1, 'upper'));
fprintf('  chi2cdf(3.84, 1, "upper")     = %.8f\n', chi2cdf(3.84, 1, 'upper'));
fprintf('  tcdf(2.0, 10, "upper")        = %.8f\n', tcdf(2.0, 10, 'upper'));
fprintf('  fcdf(2, 5, 10, "upper")       = %.8f (expect 0.16419495)\n', fcdf(2, 5, 10, 'upper'));
fprintf('  betacdf(0.5, 2, 3, "upper")   = %.8f (expect 0.31250000)\n', betacdf(0.5, 2, 3, 'upper'));
fprintf('  gamcdf(2, 1, 1, "upper")      = %.8f (expect exp(-2))\n', gamcdf(2, 1, 1, 'upper'));
fprintf('  expcdf(1, 1, "upper")         = %.8f (expect exp(-1))\n', expcdf(1, 1, 'upper'));
fprintf('  raylcdf(1, 1, "upper")        = %.8f (expect exp(-0.5))\n', raylcdf(1, 1, 'upper'));
fprintf('  logncdf(1, 0, 1, "upper")     = %.8f (expect 0.5)\n', logncdf(1, 0, 1, 'upper'));
fprintf('  wblcdf(1, 1, 1, "upper")      = %.8f (expect exp(-1))\n', wblcdf(1, 1, 1, 'upper'));
fprintf('  unifcdf(0.3, 0, 1, "upper")   = %.8f (expect 0.7)\n', unifcdf(0.3, 0, 1, 'upper'));

fprintf('\n=== discrete distributions ===\n');
fprintf('  unidcdf(3, 5, "upper")        = %.8f\n', unidcdf(3, 5, 'upper'));
fprintf('  binocdf(2, 5, 0.3, "upper")   = %.8f\n', binocdf(2, 5, 0.3, 'upper'));
fprintf('  poisscdf(2, 3, "upper")       = %.8f\n', poisscdf(2, 3, 'upper'));

fprintf('\n=== sanity: lower tail unchanged ===\n');
fprintf('  normcdf(0)                    = %.8f (expect 0.5)\n', normcdf(0));

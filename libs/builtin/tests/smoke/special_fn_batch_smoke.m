clear

import compat.*

% Special-function batch — audit ТЗ closure 2026-05-09. 17 functions.
% bessel{j,y,i,k,h} + beta/betainc/betaincinv/betaln +
% gamma/gammainc/gammaincinv/gammaln + erf/erfc/erfinv/erfcinv.

fprintf('besselj(0,1)         = %.15f\n', besselj(0,1));
fprintf('bessely(0,1)         = %.15f\n', bessely(0,1));
fprintf('besseli(0,1)         = %.15f\n', besseli(0,1));
fprintf('besselk(0,1)         = %.15f\n', besselk(0,1));
h = besselh(0, 1, 1);
fprintf('besselh(0,1,1)       = %.15f + %.15fi\n', real(h), imag(h));
fprintf('beta(2,3)            = %.15f  (expect 1/12)\n', beta(2,3));
fprintf('betainc(0.5,2,3)     = %.15f\n', betainc(0.5,2,3));
fprintf('betaln(2,3)          = %.15f\n', betaln(2,3));
fprintf('gamma(0.5)           = %.15f  (expect sqrt(pi))\n', gamma(0.5));
fprintf('gamma(5)             = %.15f  (expect 24 = 4!)\n', gamma(5));
fprintf('gammainc(2,2)        = %.15f\n', gammainc(2,2));
fprintf('gammaln(10)          = %.15f\n', gammaln(10));
fprintf('erf(1)               = %.15f\n', erf(1));
fprintf('erfc(1)              = %.15f\n', erfc(1));
fprintf('erfinv(0.5)          = %.15f\n', erfinv(0.5));
fprintf('erfcinv(0.3)         = %.15f\n', erfcinv(0.3));

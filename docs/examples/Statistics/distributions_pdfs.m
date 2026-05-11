% distributions_pdfs.m
% Plot the probability density of four classic continuous
% distributions side-by-side, varying the shape/scale parameter.

import compat.*;

clear
close all

x = linspace(0, 10, 400);

figure;

% Normal: shift mu, fix sigma.
subplot(2,2,1);
plot(x, normpdf(x, 2, 1), x, normpdf(x, 5, 1), x, normpdf(x, 7, 1.5));
title('Normal: \mu varies, \sigma=1 (last 1.5)');
legend('\mu=2', '\mu=5', '\mu=7'); grid on;

% Exponential: vary mean.
subplot(2,2,2);
plot(x, exppdf(x, 1), x, exppdf(x, 2), x, exppdf(x, 4));
title('Exponential: rate parameter');
legend('\mu=1', '\mu=2', '\mu=4'); grid on;

% Gamma: shape parameter.
subplot(2,2,3);
plot(x, gampdf(x, 1, 2), x, gampdf(x, 3, 2), x, gampdf(x, 7, 1));
title('Gamma: shape sweep');
legend('k=1, \theta=2', 'k=3, \theta=2', 'k=7, \theta=1'); grid on;

% Chi-square: degrees of freedom.
subplot(2,2,4);
plot(x, chi2pdf(x, 2), x, chi2pdf(x, 4), x, chi2pdf(x, 7));
title('\chi^2: degrees of freedom');
legend('df=2', 'df=4', 'df=7'); grid on;

sgtitle('Common continuous distributions — PDFs');

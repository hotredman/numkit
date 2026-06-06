% qqplot — Quantile-Quantile plot vs the standard normal
% Sample quantiles on Y, theoretical normal quantiles on X.
% A reference line through the 25th + 75th sample percentiles makes
% deviation from normality visible: heavy tails curve away from the
% line at the extremes.
import compat.*;

clear
close all

% Mostly-normal sample with a small heavy-tail contamination.
x = [randn(1, 90), 4 + randn(1, 10)];

figure
qqplot(x)
title('qqplot — Sample with a Heavy-Tail Contaminant')
xlabel('Theoretical normal quantile')
ylabel('Sample quantile')
grid on

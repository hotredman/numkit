% errorbar — Plot with symmetric error bars
% errorbar(x, y, e) draws each (x_i, y_i) with a vertical bar of
% half-height e_i. errorbar(x, y, neg, pos) supports asymmetric
% intervals.
import compat.*;

clear
close all

x = 1:10;
y = sqrt(x);
% Confidence intervals shrink with x (more data → tighter).
e = 0.4 ./ sqrt(x);

figure
errorbar(x, y, e)
title('Mean \pm 1 SE')
xlabel('Sample')
ylabel('y')
grid on

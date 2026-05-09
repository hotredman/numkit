% boxplot — Tukey box + whiskers + outliers
% Each box spans Q1..Q3 (the inter-quartile range) with the median
% as a red line; whiskers extend to the most extreme in-fence value
% (Q1 - 1.5·IQR / Q3 + 1.5·IQR), points beyond are outliers.
import compat.*;

clear
close all

% Three groups with different spread + a couple of outliers.
% randn(R, C) = R×C standard normal.
g1 = 2  + 1.0 * randn(1, 40);
g2 = 4  + 1.5 * randn(1, 40);
g3 = 6  + 0.8 * randn(1, 40);
g3(end) = 12;     % seeded outlier

% boxplot on a matrix → one box per column.
M = [g1; g2; g3]';

figure
boxplot(M)
title('Three Sample Groups')
xlabel('Group')
ylabel('Value')
grid on

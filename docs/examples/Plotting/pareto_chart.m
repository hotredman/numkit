% pareto — 80/20 chart
% Bars sorted descending by value + a cumulative percent line on top.
% Classic quality-control visualisation — the line crossing 80% tells
% you which categories cover the bulk of the issue count.
import compat.*;

clear
close all

% Defect counts by category.
counts = [28 14 9 7 5 3 2 1];

figure
pareto(counts)
title('Pareto Chart — Defect Count by Category')
xlabel('Category (sorted)')
ylabel('Count / Cumulative %')
grid on

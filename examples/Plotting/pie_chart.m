% pie — Wedge chart with auto-labels
% pie(X) shows each X(i) as a wedge sized proportionally; auto labels
% are percentages. pie(X, explode) displaces selected wedges by ~10%
% along their bisector — useful to call out a slice.
import compat.*;

clear
close all

% Market share by browser.
share = [55 18 12 8 4 3];

% Highlight Chrome (1st) and Edge (3rd).
explode = [1 0 1 0 0 0];

figure
pie(share, explode)
legend('Chrome', 'Safari', 'Edge', 'Firefox', 'Opera', 'Other', ...
       'Location', 'eastoutside')
title('Browser Market Share')

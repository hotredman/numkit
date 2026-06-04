% Many Figures — 20 large scatter plots (WebGL context-limit stress test)
% Opens 20 separate figures, each a 50k-point scatter cloud. A naive design
% with one WebGL context per preview card would blow past the browser's
% ~16-context cap and blank the oldest thumbnails; the shared-context preview
% rasterizer draws every card from ONE context, so all 20 stay crisp and match
% their windows. Watch the Figures panel fill with thumbnails.
import compat.*;

clear
close all

nFig = 20;
nPts = 50000;
for k = 1:nFig
    a = k / nFig;
    x = randn(1, nPts) * (0.6 + a);
    y = randn(1, nPts) * (1.6 - a) + 0.4 * x;   % per-figure spread + correlation
    figure
    scatter(x, y)
    title(sprintf('Cloud %d - 50k points', k))
    xlabel('x')
    ylabel('y')
end

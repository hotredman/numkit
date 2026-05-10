clear;
import compat.*;

% tiledlayout / nexttile — modern subplot API.

tiledlayout(2, 2);

nexttile;
plot(linspace(0, 2*pi, 50), sin(linspace(0, 2*pi, 50)));
title('sin');

nexttile;
plot(linspace(0, 2*pi, 50), cos(linspace(0, 2*pi, 50)));
title('cos');

nexttile;
plot(linspace(0, 2*pi, 50), tan(linspace(0, 2*pi, 50)));
title('tan');

nexttile;
plot(linspace(0, 2*pi, 50), sin(linspace(0, 2*pi, 50)) .* cos(linspace(0, 2*pi, 50)));
title('sin·cos');

fprintf('tiledlayout(2,2) + 4× nexttile OK — 4-cell grid\n');

% Jump-to-cell variant.
figure;
tiledlayout(1, 3);
nexttile(2);   % skip cell 1
plot([1 2 3], [1 2 3]);
fprintf('nexttile(2) — cell-skip OK\n');

fprintf('tiledlayout smoke DONE\n');

% tiledlayout / nexttile — Modern subplot API
%
% Equivalent to subplot, but with cleaner cell-stepping semantics
% and forward-compat with MATLAB R2019+.

import compat.*;

clear
close all

t = linspace(0, 2*pi, 50);

tiledlayout(2, 2);

nexttile;
plot(t, sin(t), 'b-', 'LineWidth', 2);
title('sin(t)');
xlabel('t'); ylabel('y');

nexttile;
plot(t, cos(t), 'r-', 'LineWidth', 2);
title('cos(t)');
xlabel('t'); ylabel('y');

nexttile;
plot(t, sin(t) .* cos(t), 'g-', 'LineWidth', 2);
title('sin(t)·cos(t)');
xlabel('t'); ylabel('y');

nexttile;
plot(t, sin(2*t), 'm-', 'LineWidth', 2);
title('sin(2t)');
xlabel('t'); ylabel('y');

sgtitle('tiledlayout(2, 2) — 4 trig waveforms');

% --- Cell-skip variant ---
figure;
tiledlayout(1, 3);
nexttile(2);           % skip cell 1, draw in 2
plot(t, sin(t), 'b-');
title('Drawn in cell 2 of 1×3');
xlabel('t'); ylabel('y');

fprintf('tiledlayout demo: 2x2 trig grid + 1x3 cell-skip example\n');

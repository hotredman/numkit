% animatedline — Incremental Plot
%
% animatedline is the modern alternative to "hold on; plot(...)".
% Build the line one point at a time, useful for simulation loops.

import compat.*;

clear
close all

% --- 1. Real-time sine sweep ---
h = animatedline;
for k = 1:60
    t = k * 0.1;
    addpoints(h, t, sin(t));
end
drawnow;
title('animatedline + addpoints, 60 samples');

% --- 2. Seeded form: initialise with a starting curve ---
figure;
h2 = animatedline(0:9, (0:9).^2);
% Extend the same line.
for k = 10:15
    addpoints(h2, k, k^2);
end
title('animatedline(x0, y0) seeded, then extended to k=15');

% --- 3. getpoints — read back accumulated samples ---
[xv, yv] = getpoints(h);
fprintf('animatedline accumulated %d points\n', length(xv));
fprintf('  first = (%g, %g), last = (%g, %g)\n', ...
        xv(1), yv(1), xv(end), yv(end));

% --- 4. clearpoints — erase and rebuild ---
clearpoints(h);
for k = 1:30
    addpoints(h, k, k * cos(k * 0.5));
end
title('Same handle after clearpoints + 30 new samples');

fprintf('animatedline demo: 3 figures + console summary\n');

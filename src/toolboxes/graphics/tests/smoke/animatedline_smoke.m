clear;
import compat.*;

% animatedline — incremental line plot.

h = animatedline;
fprintf('h class = %s (expect double)\n', class(h));

for k = 1:20
    addpoints(h, k, sin(k * 0.3));
end
drawnow;
fprintf('addpoints loop OK — 20 points appended\n');

[xv, yv] = getpoints(h);
fprintf('getpoints len = %d (expect 20)\n', length(xv));
fprintf('xv(20) = %g, yv(20) = %g\n', xv(20), yv(20));

clearpoints(h);
[xv2, yv2] = getpoints(h);
fprintf('after clearpoints len = %d (expect 0)\n', length(xv2));

% Seeded form.
figure;
h2 = animatedline(1:5, [1 4 9 16 25]);
fprintf('animatedline(x0, y0) seeded OK\n');

fprintf('animatedline smoke DONE\n');

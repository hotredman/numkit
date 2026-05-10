% inpolygon — Point-in-Polygon Test
%
% Ray-casting (crossing-number) test: which query points lie INSIDE
% the closed polygon defined by (xv, yv).

import compat.*;

clear
close all

% --- L-shaped polygon ---
xv = [0  3  3  1  1  0  0];
yv = [0  0  1  1  3  3  0];

% Grid of query points.
[Xq, Yq] = meshgrid(linspace(-0.5, 3.5, 30), linspace(-0.5, 3.5, 30));
in = inpolygon(Xq(:), Yq(:), xv, yv);

% Visualise — green inside, red outside.
inIdx = find(in);
outIdx = find(~in);
figure;
plot(Xq(outIdx), Yq(outIdx), 'r.');
hold on;
plot(Xq(inIdx),  Yq(inIdx),  'g.');
plot(xv, yv, 'k-', 'LineWidth', 1.5);
title('inpolygon — L-shaped polygon, 900 query points');
xlabel('x'); ylabel('y');
legend('outside', 'inside', 'polygon');

fprintf('%d of %d points classified inside\n', ...
        sum(in), numel(in));

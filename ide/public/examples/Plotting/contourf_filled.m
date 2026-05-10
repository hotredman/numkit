% contourf — Filled Contour Bands
%
% contourf is contour's sibling that paints each band between two
% adjacent isolevels. Uses an HSL ramp from blue (data min) to red
% (data max).

import compat.*;

clear
close all

% Classic "peaks"-like function on a 60×60 grid.
[X, Y] = meshgrid(linspace(-3, 3, 60));
Z = 3 * (1 - X).^2 .* exp(-X.^2 - (Y + 1).^2) ...
  - 10 * (X/5 - X.^3 - Y.^5) .* exp(-X.^2 - Y.^2) ...
  - exp(-(X + 1).^2 - Y.^2) / 3;

figure;
contourf(Z);
title('contourf(Z) — default 10 levels');

figure;
contourf(Z, 20);
title('contourf(Z, 20) — finer banding');

% Explicit X / Y grid.
figure;
contourf(X, Y, Z, [-6 -2 0 2 6]);
title('contourf(X, Y, Z, [-6 -2 0 2 6]) — custom levels');

fprintf('contourf demo: peaks-like surface with three banding densities\n');

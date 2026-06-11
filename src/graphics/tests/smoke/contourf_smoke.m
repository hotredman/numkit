clear;
import compat.*;

% contourf — filled contour bands.
[X, Y] = meshgrid(linspace(-3, 3, 30));
Z = X .* exp(-X.^2 - Y.^2);

contourf(Z);
fprintf('contourf(Z) OK — should emit ≥ 11 polygon datasets (10 levels + base)\n');

figure;
contourf(Z, 5);
fprintf('contourf(Z, 5) OK — 5 levels + base = 6 bands\n');

figure;
contourf(X, Y, Z);
fprintf('contourf(X, Y, Z) OK — explicit grid\n');

fprintf('contourf smoke DONE\n');

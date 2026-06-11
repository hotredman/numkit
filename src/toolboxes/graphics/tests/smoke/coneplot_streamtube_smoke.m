clear;
import compat.*;

% coneplot / streamtube — 3-D vector-field visualisations.

% Build a simple "uniform flow along X" field.
U = ones(4, 4, 4);
V = zeros(4, 4, 4);
W = zeros(4, 4, 4);

coneplot(U, V, W);
fprintf('coneplot(U, V, W) OK — cones at every grid point\n');

figure;
streamtube(U, V, W, [1 1 1], [2 2 3], [2 3 2]);
fprintf('streamtube — three seeds, tubes wrapping streamlines\n');

% User-position cones.
figure;
x = linspace(0, 3, 4);
y = linspace(0, 3, 4);
z = linspace(0, 3, 4);
coneplot(x, y, z, U, V, W, [0.5 1.5 2.5], [1 1 1], [1 2 3]);
fprintf('coneplot at user positions OK\n');

fprintf('coneplot / streamtube smoke DONE\n');

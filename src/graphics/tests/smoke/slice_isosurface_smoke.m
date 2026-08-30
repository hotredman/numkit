clear;

% slice / isosurface — 3-D scalar volume visualisations.

% Build a simple gradient volume.
V = zeros(5, 5, 5);
V(:,:,1) = 1; V(:,:,2) = 2; V(:,:,3) = 3;
V(:,:,4) = 4; V(:,:,5) = 5;
fprintf('size(V) = [%d %d %d]\n', size(V,1), size(V,2), size(V,3));

% Slice at mid-planes.
slice(V, [3], [3], [3]);
fprintf('slice OK — 3 axis-aligned planes through (3, 3, 3)\n');

% Isosurface at mid-value.
figure;
isosurface(V, 2.5);
fprintf('isosurface(V, 2.5) OK — marching-cubes mesh\n');

% Explicit grid form.
figure;
x = linspace(-1, 1, 5);
y = linspace(-1, 1, 5);
z = linspace(-1, 1, 5);
slice(x, y, z, V, [0], [0], [0]);
fprintf('slice with explicit X/Y/Z OK\n');

fprintf('slice / isosurface smoke DONE\n');

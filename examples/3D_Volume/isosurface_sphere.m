% isosurface — Marching Cubes
%
% isosurface(V, iso) extracts the surface where V equals `iso` from
% a 3-D scalar volume. Uses the standard 256-case Paul-Bourke lookup
% tables; vertices on cube edges get linear-interpolated positions.
% Vertex normals are smoothed across cube boundaries for a Gouraud
% look.

import compat.*;

clear
close all

% Spherical level set:  V(i,j,k) = distance to volume centre.
M = 24; N = 24; P = 24;
V = zeros(M, N, P);
for k = 1:P
    for j = 1:N
        for i = 1:M
            V(i, j, k) = sqrt((i - M/2)^2 + (j - N/2)^2 + (k - P/2)^2);
        end
    end
end

% Sphere of radius 8.
isosurface(V, 8);
title('Isosurface at radius = 8 (sphere)');

% Smaller isovalue → smaller sphere.
figure;
isosurface(V, 4);
title('Isosurface at radius = 4 (inner sphere)');

% Two-blobs volume.
W = zeros(M, N, P);
for k = 1:P
    for j = 1:N
        for i = 1:M
            r1 = (i - 8)^2 + (j - 12)^2 + (k - 12)^2;
            r2 = (i - 16)^2 + (j - 12)^2 + (k - 12)^2;
            W(i, j, k) = exp(-r1 / 20) + exp(-r2 / 20);
        end
    end
end
figure;
isosurface(W, 0.5);
title('Two-blob isosurface — Gaussian sum at level 0.5');

fprintf('isosurface demo: 3 figures (sphere R=8, R=4, twin-blob)\n');

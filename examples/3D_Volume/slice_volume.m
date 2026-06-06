% slice — Axis-Aligned Cross Sections of a 3-D Volume
%
% Reveals a 3-D scalar field by slicing it with planes perpendicular
% to each axis. Each slice gets a per-cell colormap mapping the
% volume's value through an HSL ramp (blue→red across data range).

import compat.*;

clear
close all

% A 3-D "tent" centred at the volume midpoint.
M = 20; N = 20; P = 20;
V = zeros(M, N, P);
for k = 1:P
    for j = 1:N
        for i = 1:M
            r2 = (i - M/2)^2 + (j - N/2)^2 + (k - P/2)^2;
            V(i, j, k) = exp(-r2 / 30);
        end
    end
end

% Three orthogonal slices through the centre.
slice(V, [N/2], [M/2], [P/2]);
title('3 orthogonal slices through volume centre');

% Multiple slices along one axis.
figure;
slice(V, [5 10 15], [], []);
title('Three X-planes at x = 5, 10, 15');

fprintf('slice demo: 2 figures (orthogonal + multi-X)\n');
fprintf('Volume extent: %.3f .. %.3f\n', min(V(:)), max(V(:)));

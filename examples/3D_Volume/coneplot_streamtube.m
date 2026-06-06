% coneplot + streamtube — 3-D Vector Fields
%
% Two complementary visualisations of a (U, V, W) field:
%   • coneplot — small cone arrows at grid points / user positions
%   • streamtube — tubes wrapping the streamlines through each seed

import compat.*;

clear
close all

% --- Build a swirl field around the Z axis ---
%   U(x, y, z) = -y     (counter-clockwise rotation)
%   V(x, y, z) =  x
%   W(x, y, z) =  0.3   (slight upward drift)
M = 5; N = 5; P = 5;
U = zeros(M, N, P);
Vf = zeros(M, N, P);
W = zeros(M, N, P);
for k = 1:P
    for j = 1:N
        for i = 1:M
            U(i, j, k) = -(i - M/2);
            Vf(i, j, k) =  (j - N/2);
            W(i, j, k) = 0.3;
        end
    end
end

% Cone field — arrow at every grid point.
coneplot(U, Vf, W);
title('coneplot — swirl + upward drift on 5×5×5 grid');

% Streamlines from four seeds near the bottom.
figure;
streamtube(U, Vf, W, [2 2 4 4], [2 4 2 4], [1 1 1 1]);
title('streamtube — 4 seeds spiralling upward');

% Combined: cones at user positions over a streamtube backbone.
% (X/Y/Z grid coords are required when supplying Cx/Cy/Cz.)
figure;
streamtube(U, Vf, W, [3], [3], [1]);
hold on;
xv = 1:M; yv = 1:N; zv = 1:P;
coneplot(xv, yv, zv, U, Vf, W, ...
         [2 3 4 4], [2 3 4 4], [2 3 3 4]);
title('streamtube + user-positioned coneplot overlay');

fprintf('coneplot/streamtube demo: 3 figures (cones, tubes, overlay)\n');

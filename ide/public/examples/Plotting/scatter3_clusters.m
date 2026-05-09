% scatter3 Clusters — 3-D scatter of synthetic point clouds
% Hold-on lets us layer three Gaussian blobs in distinct positions.
% Each cluster gets its own color and the orbit camera highlights
% how the clusters separate in space.
import compat.*;

clear
close all

% Three clusters at different centres. randn(N) is N×N standard
% normal; we take a 1×N slice and shift.
N = 60;
c1x = 0.5 * randn(1, N) - 2;
c1y = 0.5 * randn(1, N) - 2;
c1z = 0.5 * randn(1, N);

c2x = 0.5 * randn(1, N) + 2;
c2y = 0.5 * randn(1, N) - 1;
c2z = 0.5 * randn(1, N) + 1;

c3x = 0.5 * randn(1, N);
c3y = 0.5 * randn(1, N) + 2;
c3z = 0.5 * randn(1, N) + 2;

figure
scatter3(c1x, c1y, c1z)
hold on
scatter3(c2x, c2y, c2z)
scatter3(c3x, c3y, c3z)
title('Three 3-D Gaussian Clusters')
xlabel('X')
ylabel('Y')
zlabel('Z')
grid on

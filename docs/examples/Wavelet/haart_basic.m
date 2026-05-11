% haart_basic.m
% Haar wavelet transform — the simplest orthogonal wavelet, often used
% to introduce the multi-level decomposition idea. Shows analysis +
% reconstruction round-trip and the per-level detail energies.

import compat.*;

clear
close all

% A piecewise-constant signal is "ideal" for Haar — its details light
% up only at the discontinuities.
N = 64;
x = zeros(N, 1);
x(1:16)    = 1.0;
x(17:32)   = 0.3;
x(33:48)   = 0.7;
x(49:end)  = 0.1;
x = x + 0.05 * randn(N, 1);

% 3-level Haar decomposition.
[a, d] = haart(x, 3);            % a: approx; d: cell of details

% Reconstruction round-trip.
xr = ihaart(a, d);
err = max(abs(x - xr));
fprintf('Max round-trip error after ihaart: %.3e\n', err);

% Energy per detail level (||d_k||^2).
fprintf('Detail energies:\n');
for k = 1:numel(d)
    fprintf('  level %d  ||d||^2 = %.4f\n', k, sum(d{k}.^2));
end

figure;
subplot(2,1,1); stairs(x);  title('Input piecewise signal');
                xlabel('sample'); grid on;
subplot(2,1,2); stairs(xr); title('Reconstruction (ihaart)');
                xlabel('sample'); grid on;
sgtitle('Haar wavelet round-trip');

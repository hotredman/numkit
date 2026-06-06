% Ten-Million-Point Line — GPU level-of-detail
% A 10M-sample noisy multi-tone signal. Above ~1M points the engine keeps the
% raw data engine-side and the viewer draws a GPU LOD tile (~4× the plot width
% in points), refetched only on zoom — so the plot opens instantly and pans at
% any zoom with no overdraw. Bump n to 50e6 to stress the LOD further.
import compat.*;

clear
close all

n = 10000000;                      % 10M samples
t = linspace(0, 100, n);
y = sin(t) + 0.3 * sin(50 * t) + 0.05 * randn(1, n);

figure
plot(t, y)
title('10M-point line — GPU LOD (zoom in to reveal detail)')
xlabel('t')
ylabel('amplitude')
grid on

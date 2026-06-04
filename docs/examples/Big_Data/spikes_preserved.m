% Spikes Preserved — downsampling that doesn't hide outliers
% A long, mostly-smooth signal with three sharp spikes (~500k points). The
% plot's default M4 downsampling keeps the min and max of every pixel
% column, so the spikes stay visible at any zoom level — naive subsampling
% would drop them. Zoom into a spike to watch the full sample detail appear.
import compat.*;

clear
close all

n = 500000;
t = linspace(0, 10, n);
y = 0.2 * sin(2*pi*1.5*t);
y(round(n*0.25)) =  4;        % sharp positive spike
y(round(n*0.60)) = -3;        % sharp negative spike
y(round(n*0.85)) =  5;

figure
plot(t, y)
title('500k points with 3 spikes — all preserved by M4 downsampling')
xlabel('Time (s)')
ylabel('Amplitude')
grid on

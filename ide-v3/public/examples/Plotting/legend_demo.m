% Legend Demo — Labeling multiple datasets
% Use legend() after plotting to add a legend box.
% Pull toolbox functions (signal, stats, graphics, io) into scope so we
% can call them by short name (fft, butter, plot, std, ...). Without this,
% we'd need fully qualified names like signal.transforms.fft(...).
import compat.*;

clear
close all

x = linspace(0, 4*pi, 200);

figure
hold on
plot(x, sin(x))
plot(x, 0.5 * sin(2*x))
plot(x, 0.25 * sin(4*x))
hold off
title('Harmonics')
xlabel('Angle (rad)')
ylabel('Amplitude')
legend('fundamental', '2nd harmonic', '4th harmonic')
grid on

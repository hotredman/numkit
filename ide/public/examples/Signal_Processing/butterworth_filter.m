% Butterworth Filter — Design and apply a lowpass filter
% Use butter() to design, filter() to apply, and freqz() to visualize.
clear
close all

% Design a 4th-order lowpass at 0.3*Nyquist
[b, a] = butter(4, 0.3);

% Frequency response
[H, W] = freqz(b, a, 256);

figure(1)
plot(W/pi, abs(H), 'b-', 'LineWidth', 1.5)
title('Butterworth LP Magnitude Response')
xlabel('Normalized Frequency (x pi rad/sample)')
ylabel('|H(w)|')
grid on

% Apply to a test signal: 10 Hz + 80 Hz, Fs = 256
Fs = 256;
t = 0:1/Fs:1-1/Fs;
x = sin(2*pi*10*t) + 0.5*sin(2*pi*80*t);
y = filter(b, a, x);

figure(2)
hold on
plot(t, x, 'b-')
plot(t, y, 'r-', 'LineWidth', 1.5)
hold off
title('Lowpass Filtering (butter + filter)')
xlabel('Time (s)')
ylabel('Amplitude')
legend('original', 'filtered')
grid on

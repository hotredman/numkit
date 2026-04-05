% FIR Filter — Design a lowpass FIR with fir1
% Compare the filter impulse response and frequency response.
clear
close all

% 30-tap lowpass FIR, cutoff at 0.4*Nyquist
N = 30;
b = fir1(N, 0.4);

% Impulse response
figure(1)
stem(0:N, b)
title('FIR Impulse Response (N=30, Wn=0.4)')
xlabel('Tap')
ylabel('Coefficient')
grid on

% Frequency response
[H, W] = freqz(b, [1], 512);

figure(2)
plot(W/pi, abs(H), 'b-', 'LineWidth', 1.5)
title('FIR Magnitude Response')
xlabel('Normalized Frequency (x pi rad/sample)')
ylabel('|H(w)|')
grid on

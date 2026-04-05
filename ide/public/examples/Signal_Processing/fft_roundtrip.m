% FFT Roundtrip — fft, fftshift, ifftshift, ifft
% Transform a signal to frequency domain, shift it,
% then reconstruct and verify.
clear
close all

N = 128;
t = linspace(0, 1, N);
x = sin(2*pi*5*t) + 0.5*cos(2*pi*20*t);

% Forward FFT
X = fft(x);

% Shift zero-frequency to center
Xs = fftshift(X);

% Undo the shift
Xu = ifftshift(Xs);

% Reconstruct
x_rec = ifft(Xu);

% Frequency axis (centered)
f = linspace(-N/2, N/2-1, N);

figure(1)
plot(f, abs(Xs) / N, 'b-', 'LineWidth', 1.5)
title('Centered Spectrum (fftshift)')
xlabel('Frequency bin')
ylabel('|X| / N')
grid on

figure(2)
hold on
plot(t, x, 'b-', 'LineWidth', 1.5)
plot(t, x_rec, 'r--', 'LineWidth', 1.5)
hold off
title('Original vs Reconstructed (ifft)')
xlabel('Time (s)')
ylabel('Amplitude')
legend('original', 'ifft')
grid on

disp('Max reconstruction error:')
disp(max(abs(x - x_rec)))

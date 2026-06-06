% mfcc_features.m
% Extract Mel-frequency cepstral coefficients from a synthetic
% harmonic source. MFCCs are the workhorse feature for speech/audio
% classification.

import compat.*;

clear
close all

fs = 16000;
t  = (0:1/fs:1)';

% Synthetic vowel-like source: fundamental + 4 harmonics, with slow
% amplitude modulation to vary the spectral envelope across time.
f0  = 200;
mod = 0.5 + 0.5 * sin(2*pi*3*t);    % 3-Hz amplitude modulation
y = mod .* (sin(2*pi*f0*t) + ...
            0.6 * sin(2*pi*2*f0*t) + ...
            0.4 * sin(2*pi*3*f0*t) + ...
            0.3 * sin(2*pi*4*f0*t));
y = y / max(abs(y));

% MFCC: 13 coefficients per ~25 ms frame (default).
coeffs = mfcc(y, fs);

fprintf('MFCC matrix size: %d frames x %d coefficients\n', ...
        size(coeffs, 1), size(coeffs, 2));

figure;
subplot(2, 1, 1);
plot(t, y); xlabel('time (s)'); ylabel('amplitude');
title('Synthetic harmonic signal (200 Hz fundamental + harmonics + AM)');
grid on;

subplot(2, 1, 2);
imagesc(coeffs'); axis xy; colorbar;
xlabel('frame'); ylabel('MFCC index');
title('MFCC over time');

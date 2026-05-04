clear

import compat.*

% FSK round-trip
% M=4, freq_sep=100 Hz, nsamp=10, fs=2000 Hz
data = [0 1 2 3 0 2 1 3];
y = fskmod(data, 4, 100, 10, 2000);
fprintf('--- fskmod size = %d (expect 80 = 8·10) ---\n', length(y));
out = fskdemod(y, 4, 100, 10, 2000);
fprintf('FSK round-trip: '); disp(out');
fprintf('  expect: [0 1 2 3 0 2 1 3]\n\n');

% FSK with discontinuous phase mode
y2 = fskmod(data, 4, 100, 10, 2000, 'discont');
out2 = fskdemod(y2, 4, 100, 10, 2000);
fprintf('FSK discont round-trip: '); disp(out2');
fprintf('  expect: [0 1 2 3 0 2 1 3]\n\n');

% OFDM: 8-FFT, 2 cyclic-prefix, 3 symbols
nfft = 8;
cplen = 2;
% Simple pattern: each subcarrier holds a different complex value
in = (0:nfft-1)' * (1:3);   % nfft × 3 (each col is 0..7 scaled by 1, 2, 3)
y_ofdm = ofdmmod(in, nfft, cplen);
fprintf('--- ofdmmod size = %d (expect %d = (nfft+cp)·Nsym) ---\n', ...
    length(y_ofdm), (nfft + cplen) * 3);

% Demod and compare
out_ofdm = ofdmdemod(y_ofdm, nfft, cplen);
fprintf('OFDM round-trip max abs error: %.2e\n', max(max(abs(out_ofdm - in))));
fprintf('  expect: ≪ 1e-10\n');

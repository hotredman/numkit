clear

% awgn — measure SNR
rng(42);
s = randn(1000, 1);
y = awgn(s, 10);   % 10 dB
sig_pow = mean(s.^2);
noise_pow = mean((y - s).^2);
fprintf('--- awgn(s, 10 dB) ---\n');
fprintf('signal power = %.4f, noise power = %.4f, measured SNR = %.4f dB\n', ...
    sig_pow, noise_pow, 10*log10(sig_pow / noise_pow));
fprintf('  expect: SNR ≈ 10 dB\n\n');

% wgn — generate WGN with given dBW
n = wgn(1000, 1, 0);   % 0 dBW = 1 W
p = mean(n.^2);
fprintf('--- wgn(1000, 1, 0 dBW) avg power = %.4f ---\n', p);
fprintf('  expect: ≈ 1.0\n\n');

% bsc with p=0
b = bsc([0 1 0 1 1 0 0 1 1 0], 0);
fprintf('--- bsc with p=0 (no flips) ---\n');
disp(b');
fprintf('  expect: [0 1 0 1 1 0 0 1 1 0]\n\n');

% qfunc / qfuncinv round-trip
xs = [0 0.5 1.0 1.5 2.0 2.5];
qs = qfunc(xs);
fprintf('--- qfunc([0 0.5 1.0 1.5 2.0 2.5]) ---\n');
disp(qs);
fprintf('  expect: [0.5 0.3085 0.1587 0.0668 0.0228 0.0062]\n\n');

% Round-trip
fprintf('qfuncinv(qfunc(1.5)) = %.6f (expect 1.5)\n\n', qfuncinv(qfunc(1.5)));

% berawgn for BPSK at various Eb/No
EbNo = 0:2:10;
ber = berawgn(EbNo, 'psk', 2);
fprintf('--- berawgn BPSK at Eb/No = 0..10 dB ---\n');
fprintf('Eb/No: '); disp(EbNo);
fprintf('BER:   '); disp(ber);
fprintf('  expect: 0.0786, 0.0375, 0.0125, 0.0024, 0.000191, 0.0000039\n\n');

% berawgn for QAM
ber16 = berawgn(10, 'qam', 16);
fprintf('berawgn(Eb/No=10, QAM-16) = %.6e (expect ~ 1.8e-3)\n\n', ber16);

% convertSNR
es_no = convertSNR(10, 'From', 'ebno', 'To', 'esno', 'BitsPerSymbol', 4);
fprintf('--- convertSNR(10 dB Eb/No → Es/No, k=4) = %.4f dB ---\n', es_no);
fprintf('  expect: 10 + 10·log10(4) = 16.0206 dB\n');

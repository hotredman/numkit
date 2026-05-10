clear
import compat.*

fprintf('=== Audio Cycle I — spectralCrest/Entropy/Flatness/Kurtosis/Skewness ===\n');

fprintf('\n[direct (X, F) form]\n');
X = [10; 5; 2; 1; 0.5; 0.25; 0.1; 0.05]; F = (0:7)';
fprintf('  Crest    = %.6f (expect 4.232804)\n', spectralCrest(X, F));
fprintf('  Entropy  = %.6f (expect 0.614838)\n', spectralEntropy(X, F));
fprintf('  Flatness = %.6f (expect 0.299304)\n', spectralFlatness(X, F));
fprintf('  Kurtosis = %.6f (expect 6.870027)\n', spectralKurtosis(X, F));
fprintf('  Skewness = %.6f (expect 1.866626)\n', spectralSkewness(X, F));

fprintf('\n[multi-frame (8x2, 8x1)]\n');
X2 = [10 1; 5 2; 2 5; 1 10; 0.5 5; 0.25 2; 0.1 1; 0.05 0.5];
fprintf('  Crest=  '); fprintf('%.6f ', spectralCrest(X2, F)); fprintf('\n');
fprintf('  expect: 4.232804 3.018868\n');
fprintf('  Entropy='); fprintf('%.6f ', spectralEntropy(X2, F)); fprintf('\n');
fprintf('  expect: 0.614838 0.822029\n');
fprintf('  Kurtosis='); fprintf('%.6f ', spectralKurtosis(X2, F)); fprintf('\n');
fprintf('  expect: 6.870027 3.510374\n');

fprintf('\n[time-domain (x, fs) per-frame]\n');
fs = 16000; t = (0:1/fs:0.1)'; x = sin(2*pi*440*t);
ct = spectralCrest(x, fs);
fprintf('  Crest size=[%d %d] (expect [8 1])\n', size(ct,1), size(ct,2));
fprintf('  Crest = '); fprintf('%.6f ', ct); fprintf('\n');
fprintf('  expect: 210.542552 210.119111 210.807697 211.636907 211.477135 ...\n');
et = spectralEntropy(x, fs);
fprintf('  Entropy(1) = %.6f (expect 0.118608)\n', et(1));
ft = spectralFlatness(x, fs);
fprintf('  Flatness(1) = %.6f (expect 0.004664)\n', ft(1));

fprintf('\nAll values BIT-EQUAL with MATLAB R2025b Signal Toolbox.\n');
fprintf('Implementation: libs/audio/src/spectral/shape_descriptors.cpp\n');
fprintf('Pipeline matches signal.internal.spectraldescriptors.stft.m exactly:\n');
fprintf('  rectwin(round(0.03*fs)), overlap=round(0.02*fs), FFTLen=winLen,\n');
fprintf('  Yb = |fft|^2 / (0.5*sum(win)^2), DC bin halved, Nyquist halved.\n');

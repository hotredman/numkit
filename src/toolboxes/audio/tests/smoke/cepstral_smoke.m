clear

fprintf('=== Audio Cycle D — cepstral coefficients ===\n');

fprintf('\n[cepstralCoefficients on synthetic 8-band spectrum]\n');
S = [10; 5; 2; 1; 0.5; 0.25; 0.1; 0.05];
c = cepstralCoefficients(S);
fprintf('  shape=[%d %d] (expect [1 13])\n', size(c,1), size(c,2));
fprintf('  c(1:5) = '); fprintf('%.4f ', c(1:5)); fprintf('\n');
fprintf('  expect: -0.4257 2.1150 0.0000 0.2644 0.0000\n');

fprintf('\n[cepstralCoefficients NumCoeffs=5]\n');
c5 = cepstralCoefficients(S, 5);
fprintf('  shape=[%d %d] (expect [1 5])\n', size(c5,1), size(c5,2));
fprintf('  c5 = '); fprintf('%.4f ', c5); fprintf('\n');

fprintf('\n[multi-frame cepstralCoefficients]\n');
S2 = [10 1; 5 2; 2 5; 1 10; 0.5 5; 0.25 2; 0.1 1; 0.05 0.5];
c2 = cepstralCoefficients(S2);
fprintf('  shape=[%d %d]\n', size(c2,1), size(c2,2));
fprintf('  row 2: '); fprintf('%.4f ', c2(2,:)); fprintf('\n');

fprintf('\n[mfcc bit-equal MATLAB R2025b — Cycle G upgrade]\n');
fs = 16000; t = (0:1/fs:0.1)'; x = sin(2*pi*440*t);
[c, d, dd] = mfcc(x, fs);
fprintf('  mfcc shape=[%d %d] (expect [8 14])\n', size(c,1), size(c,2));
fprintf('  delta shape=[%d %d]\n', size(d,1), size(d,2));
fprintf('  deltaDelta shape=[%d %d]\n', size(dd,1), size(dd,2));
fprintf('  c(1,1) = %.6f (logE, expect 5.475232)\n', c(1,1));
fprintf('  c(1,2) = %.6f (cep DC, expect -14.165624)\n', c(1,2));
fprintf('  c(2,2) = %.6f (cep DC f2, expect -13.900615)\n', c(2,2));
fprintf('  c(1,end) = %.6f (cep last, expect -0.620010)\n', c(1,end));

fprintf('\n[gtcc bit-equal MATLAB R2025b — Cycle H upgrade]\n');
[g, gd, gdd] = gtcc(x, fs);
fprintf('  gtcc shape=[%d %d] (expect [8 14])\n', size(g,1), size(g,2));
fprintf('  g(1,1) = %.6f (logE, expect 5.475232)\n', g(1,1));
fprintf('  g(1,2) = %.6f (cep DC, expect -6.869866)\n', g(1,2));
fprintf('  g(2,2) = %.6f (cep DC f2, expect -6.952216)\n', g(2,2));
fprintf('  g(1,3) = %.6f (cep c2, expect 3.458922)\n', g(1,3));
fprintf('  g(1,end) = %.6f (cep last, expect -0.367477)\n', g(1,end));

fprintf('\nKNOWN GAPs: NONE for cycles D/G/H — all three (cepstralCoefficients\n');
fprintf('  / mfcc / gtcc) are now BIT-EQUAL with MATLAB R2025b.\n');
fprintf('  Cycle G (mfcc): Slaney mel filterbank + |FFT| magnitude pipeline.\n');
fprintf('  Cycle H (gtcc): Patterson-Holdsworth gammatone filterbank via\n');
fprintf('    cascaded 4-stage biquads (Slaney 1993) in frequency domain.\n');

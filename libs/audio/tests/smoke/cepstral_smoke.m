clear
import compat.*

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

fprintf('\n[mfcc shape check (KNOWN GAP for exact bit-equality)]\n');
fs = 16000; t = (0:1/fs:0.1)'; x = sin(2*pi*440*t);
[c, d, dd] = mfcc(x, fs);
fprintf('  mfcc shape=[%d %d] (expect [%d 14])\n', size(c,1), size(c,2), size(c,1));
fprintf('  delta shape=[%d %d]\n', size(d,1), size(d,2));
fprintf('  deltaDelta shape=[%d %d]\n', size(dd,1), size(dd,2));

fprintf('\n[gtcc shape check]\n');
[g, gd, gdd] = gtcc(x, fs);
fprintf('  gtcc shape=[%d %d]\n', size(g,1), size(g,2));

fprintf('\nKNOWN GAPs: mfcc/gtcc shape-correct, exact bit-equality with\n');
fprintf('  MATLAB R2025b deferred (MATLAB uses |FFT| magnitude into mel\n');
fprintf('  filterbank with internal designMelFilterBank normalization;\n');
fprintf('  numkit uses |FFT|^2 power via melSpectrogram from cycle C).\n');

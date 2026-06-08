clear
import compat.*

fprintf('=== Audio Cycle C — melSpectrogram + audioDelta ===\n');

fprintf('\n[melSpectrogram defaults]\n');
fs = 8000;
x = (1:0.1:80)';   % 791 samples
[S, F, T] = melSpectrogram(x, fs, 8);
fprintf('  S size = [%d %d]\n', size(S,1), size(S,2));
fprintf('  F = '); fprintf('%.4f ', F); fprintf('\n');
fprintf('  expect F(1)=164.94 F(8)=3103.72\n');
fprintf('  S(1,1) = %.6e\n', S(1,1));

fprintf('\n[melSpectrogram pure tone]\n');
fs = 16000;
t = (0:1/fs:0.1)';
xt = sin(2*pi*440*t);
[St, Ft, Tt] = melSpectrogram(xt, fs);
fprintf('  S size = [%d %d] (expect [32 8])\n', size(St,1), size(St,2));
[~, peakBand] = max(mean(St, 2));
fprintf('  peak mel band = %d at f=%.2f Hz (expect ~440 Hz)\n', peakBand, Ft(peakBand));

fprintf('\n[audioDelta ramp]\n');
xx = (1:10)';
d = audioDelta(xx);
fprintf('  size=[%d %d]\n', size(d,1), size(d,2));
fprintf('  d(9)=%g d(10)=%g (expect 2 each — MATLAB filter convention)\n', d(9), d(10));

fprintf('\n[audioDelta windowLength=5]\n');
d5 = audioDelta(xx, 5);
fprintf('  d5(5)=%g d5(10)=%g\n', d5(5), d5(10));

fprintf('\n[audioDelta multi-channel]\n');
xc = [(1:5)', (10:10:50)'];
dc = audioDelta(xc);
fprintf('  size=[%d %d]\n', size(dc,1), size(dc,2));
fprintf('  dc(5,1)=%g dc(5,2)=%g\n', dc(5,1), dc(5,2));

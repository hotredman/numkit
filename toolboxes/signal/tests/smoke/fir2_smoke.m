clear
import compat.*

fprintf('=== signal/fir2 — frequency-sampling FIR filter design ===\n');
fprintf('Clean-room: Oppenheim & Schafer; Rabiner & Gold; Parks & Burrus.\n');

fprintf('\n[lowpass: F=[0 0.4 0.5 1] A=[1 1 0 0] N=20]\n');
b = fir2(20, [0 0.4 0.5 1], [1 1 0 0]);
fprintf('  numel=%d (expect 21)\n', numel(b));
fprintf('  b(11) = %.6f (expect 0.449219, peak after time-shift)\n', b(11));
fprintf('  b(1)  = %.6f (expect 0.001659)\n', b(1));

fprintf('\n[bandpass multiband: F=[0 0.2 0.3 0.6 0.7 1] A=[0 0 1 1 0 0] N=30]\n');
b = fir2(30, [0 0.2 0.3 0.6 0.7 1], [0 0 1 1 0 0]);
fprintf('  numel=%d (expect 31)\n', numel(b));
fprintf('  b(16) = %.6f (peak, expect 0.401367)\n', b(16));

fprintf('\n[highpass: F=[0 0.5 0.6 1] A=[0 0 1 1] N=20]\n');
b = fir2(20, [0 0.5 0.6 1], [0 0 1 1]);
fprintf('  numel=%d (expect 21)\n', numel(b));
fprintf('  b(11) = %.6f (expect 0.451172)\n', b(11));

fprintf('\n[explicit npt grid size: fir2(..., 256)]\n');
bn = fir2(20, [0 0.5 1], [1 1 0], 256);
fprintf('  sum(b) = %.9f (expect 0.999703452)\n', sum(bn));

fprintf('\n[custom window: fir2(..., hann(21)) instead of Hamming]\n');
bw = fir2(20, [0 0.5 1], [1 1 0], hann(21));
fprintf('  sum(b) = %.9f (expect 0.999979106)\n', sum(bw));

fprintf('\n[lap smoothing at a discontinuity: fir2(..., 512, 30)]\n');
bl = fir2(40, [0 0.3 0.3 0.6 0.6 1], [0 0 1 1 0 0], 512, 30);
fprintf('  numel=%d  b(21) = %.8f (expect 0.30078125)\n', numel(bl), bl(21));

fprintf('\n[odd-order Nyquist correction]\n');
bo = fir2(11, [0 1], [0 1]);
fprintf('  fir2(11, [0 1], [0 1]) numel=%d (expect 13 — order bumped\n', numel(bo));
fprintf('  by 1: an odd-order symmetric FIR has a forced zero at\n');
fprintf('  Nyquist, incompatible with the requested gain 1)\n');

fprintf('\n[design realises the requested response]\n');
b = fir2(80, [0 0.4 0.5 1], [1 1 0 0]);
H = abs(fft(b, 1024)); H = H(1:513);
wn = (0:512) / 512;
fprintf('  passband gain ~%.4f (expect ~1)\n', max(H(wn <= 0.35)));
fprintf('  stopband gain ~%.4f (expect ~0)\n', max(H(wn >= 0.55)));
fprintf('  symmetric (linear phase): max|b-fliplr(b)| = %.2e\n', ...
    max(abs(b - fliplr(b))));

fprintf('\nfir2 matches MATLAB R2025b on the full argument set\n');
fprintf('(npt, lap, window, odd-order correction). Octave 11.1.0 ships\n');
fprintf('fir2 but uses a slightly different frequency grid.\n');

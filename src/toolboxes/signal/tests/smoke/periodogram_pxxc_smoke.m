clear
import compat.*

% periodogram confidence interval (3rd output pxxc) -- bugs/signal/periodogram-pxxc.
% pxxc is nf x 2 = [lower upper]; a chi-square CI with v degrees of freedom per
% bin (v=2 for interior bins, v=1 for the real DC and even-nfft Nyquist bins).
% Default coverage 0.95 when a 3rd output is requested without the name-value.

x = (1:8)';
[pxx, f, pxxc] = periodogram(x, rectwin(8), 8, 1, 'ConfidenceLevel', 0.95);

fprintf('size(pxxc) = %dx%d   (expect 5x2)\n', size(pxxc,1), size(pxxc,2));
fprintf('pxx   = '); fprintf('%.6g ', pxx); fprintf('\n');
fprintf('  expect: 162 27.3137 8 4.68629 2\n');
fprintf('lower = '); fprintf('%.6g ', pxxc(:,1)); fprintf('\n');
fprintf('  expect: 32.246 7.40434 2.16868 1.27038 0.398098\n');
fprintf('upper = '); fprintf('%.6g ', pxxc(:,2)); fprintf('\n');
fprintf('  expect: 164958 1078.83 315.983 185.099 2036.52\n');

% Per-bin DOF: DC & Nyquist (bins 1,5) ratio 1/chi2inv(.975,1)=0.19905 (v=1);
% interior bins 2/chi2inv(.975,2)=0.27109 (v=2).
fprintf('lower/pxx = '); fprintf('%.5f ', pxxc(:,1)./pxx); fprintf('\n');
fprintf('  expect:   0.19905 0.27109 0.27109 0.27109 0.19905\n');

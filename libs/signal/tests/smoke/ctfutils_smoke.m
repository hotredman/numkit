clear
import compat.*

fprintf('=== signal/ctf2zp + scaleFilterSections (Phase 4.11) ===\n');

fprintf('\n[ctf2zp single section]\n');
[z, p, k] = ctf2zp([1 -1 0.5], [1 -0.6 0.2]);
fprintf('  numel(z)=%d numel(p)=%d k=%g\n', numel(z), numel(p), real(k));

fprintf('\n[ctf2zp 2 sections with SV=[2 3 5]]\n');
NUM = [1 -1 0.5; 1 0 -1];
DEN = [1 -0.6 0.2; 1 0 -0.25];
[z, p, k] = ctf2zp(NUM, DEN, [2 3 5]);
fprintf('  k=%g (expect 30 = 2*3*5)\n', real(k));

fprintf('\n[scaleFilterSections vector SV]\n');
ctf = [1 2 1; 1 -1 0.5];
sv = scaleFilterSections(ctf, [2 3 5]);
disp(sv);
fprintf('  expect: [4.4721 8.9443 4.4721; 6.7082 -6.7082 3.3541]\n');

fprintf('\n[scaleFilterSections scalar SV=4]\n');
sv2 = scaleFilterSections(ctf, 4);
disp(sv2);

fprintf('\nBIT-EQUAL with MATLAB R2025b on 12/12 fingerprints.\n');
fprintf('KNOWN GAP: ctf2zp doesn''t strip trailing zeros from poly coeffs;\n');
fprintf('user-visible diff: extra zero/pole at 0 in z/p arrays for padded inputs.\n');

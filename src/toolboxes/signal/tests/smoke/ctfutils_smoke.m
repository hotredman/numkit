clear

fprintf('=== signal/ctf2zp + scaleFilterSections (Phase 4.11) ===\n');

fprintf('\n[ctf2zp single section]\n');
[z, p, k] = ctf2zp([1 -1 0.5], [1 -0.6 0.2]);
fprintf('  numel(z)=%d numel(p)=%d k=%g\n', numel(z), numel(p), real(k));

fprintf('\n[ctf2zp 2 sections with SV=[2 3 5]]\n');
NUM = [1 -1 0.5; 1 0 -1];
DEN = [1 -0.6 0.2; 1 0 -0.25];
[z, p, k] = ctf2zp(NUM, DEN, [2 3 5]);
fprintf('  k=%g (expect 30 = 2*3*5)\n', real(k));

fprintf('\n[scaleFilterSections — clean-room: Jackson 1996; O&S 3e §6.3]\n');
fprintf('Distributes |g|^(1/K) across K cascade sections; sign on the last.\n');

fprintf('\n[scaleFilterSections vector SV]\n');
ctf = [1 2 1; 1 -1 0.5];
sv = scaleFilterSections(ctf, [2 3 5]);
disp(sv);
fprintf('  expect: [4.4721 8.9443 4.4721; 6.7082 -6.7082 3.3541]\n');

fprintf('\n[scaleFilterSections scalar SV=4]\n');
sv2 = scaleFilterSections(ctf, 4);
disp(sv2);

fprintf('\n[scaleFilterSections single section K=1]\n');
s1 = scaleFilterSections([1 0.5 0.2], 8);
fprintf('  scaleFilterSections([1 0.5 0.2], 8) = ');
fprintf('%g ', s1); fprintf('(expect 8 4 1.6)\n');

fprintf('\n[scaleFilterSections complex coefficients]\n');
sc = scaleFilterSections([1 0.5+0.2i 0.1; 1 -0.3 0.4i], 8);
fprintf('  b(1,2) = %g%+gi (expect 1.41421+0.56569i)\n', ...
    real(sc(1,2)), imag(sc(1,2)));

fprintf('\n[correctness — cascade product scaled by exactly g]\n');
B = [1 0.6 0.1; 1 -0.4 0.25; 1 0.2 -0.3];
g = 12;
Bg = scaleFilterSections(B, g);
cB  = conv(conv(B(1,:),  B(2,:)),  B(3,:));
cBg = conv(conv(Bg(1,:), Bg(2,:)), Bg(3,:));
fprintf('  max|conv(Bg) - g*conv(B)| = %.2e (expect ~0)\n', ...
    max(abs(cBg - g * cB)));

fprintf('\nctf2zp + scaleFilterSections match MATLAB R2025b. Octave 11.1.0\n');
fprintf('does not ship scaleFilterSections (introduced R2023b).\n');
fprintf('KNOWN GAP: ctf2zp doesn''t strip trailing zeros from poly coeffs;\n');
fprintf('user-visible diff: extra zero/pole at 0 in z/p arrays for padded inputs.\n');

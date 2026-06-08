clear
import compat.*

fprintf('=== residue — s-domain partial fraction expansion ===\n');

% (-4 s + 8) / ((s+2)(s+4))
fprintf('\nresidue([-4 8], [1 6 8])  →  -4s+8 over (s+2)(s+4)\n');
[r, p, k] = residue([-4 8], [1 6 8]);
fprintf('  r ='); disp(r');
fprintf('  p ='); disp(p');
fprintf('  k ='); disp(k);
fprintf('  expect r in {-12, 8}, p in {-4, -2}, k = []\n');

% With direct term — improper-fraction-style with deg(b) == deg(a).
fprintf('\nresidue([2 5 3 6], [1 6 11 6])  →  proper after dividing out 2\n');
[r2, p2, k2] = residue([2 5 3 6], [1 6 11 6]);
fprintf('  r ='); disp(r2');
fprintf('  p ='); disp(p2');
fprintf('  k ='); disp(k2);
fprintf('  expect k = 2, poles {-1, -2, -3}, residues {3, -4, -6}\n');

% Reconstruction sanity check at s=1.
fprintf('\nHeaviside reconstruction at s=1:\n');
s = 1;
H_pfe = sum(r ./ (s - p));
H_ref = polyval([-4 8], s) / polyval([1 6 8], s);
fprintf('  PFE: %.6f   ref: %.6f   err: %g\n', H_pfe, H_ref, abs(H_pfe - H_ref));

% Repeated pole — should error.
fprintf('\nRepeated pole detection:\n');
try
    residue([1], [1 -2 1]);
    fprintf('  FAIL: should have thrown\n');
catch ME
    fprintf('  OK: %s\n', ME.message);
end


fprintf('\n=== residuez — z-domain PFE ===\n');

% Single-pole IIR: 1 / (1 - 0.5·z^-1).
fprintf('\nresiduez([1], [1 -0.5])  →  H(z) = 1 / (1 - 0.5·z^-1)\n');
[rz, pz, kz] = residuez([1], [1 -0.5]);
fprintf('  r ='); disp(rz');
fprintf('  p ='); disp(pz');
fprintf('  k ='); disp(kz);

% Two-pole IIR: poles at ±0.5.
fprintf('\nresiduez([1 0.5], [1 0 -0.25])\n');
[rz2, pz2, kz2] = residuez([1 0.5], [1 0 -0.25]);
fprintf('  r ='); disp(rz2');
fprintf('  p ='); disp(pz2');
fprintf('  k ='); disp(kz2);

% Reconstruction at z = 2.
z = 2;
H_pfe = sum(rz2 ./ (1 - pz2 .* z^-1));
H_ref = (1 + 0.5 * z^-1) / (1 - 0.25 * z^-2);
fprintf('  PFE@z=2: %.6f   ref: %.6f   err: %g\n', H_pfe, H_ref, abs(H_pfe - H_ref));

% Improper TF — should error.
fprintf('\nImproper TF (numel(b) > numel(a)):\n');
try
    residuez([1 2 3], [1 -0.5]);
    fprintf('  FAIL: should have thrown\n');
catch ME
    fprintf('  OK: %s\n', ME.message);
end

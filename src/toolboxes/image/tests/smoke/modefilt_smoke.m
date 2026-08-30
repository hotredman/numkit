clear

% image/modefilt — 2-D mode filter.
% Reference: MATLAB R2025b (smallest-on-tie semantics matching base `mode`).

fprintf('=== modefilt ===\n');

A = uint8([1 1 2 2; 1 3 2 4; 5 5 6 6; 5 7 6 8]);

B = modefilt(A);
fprintf('  default 3x3 symmetric:\n');
for r = 1:4
  fprintf('    ');
  for c = 1:4; fprintf('%2d ', B(r,c)); end
  fprintf('\n');
end
fprintf('  (e [1 1 2 2; 1 1 2 2; 5 5 6 6; 5 5 6 6])\n');

B = modefilt(A, [5 5]);
fprintf('\n  5x5 symmetric:\n');
for r = 1:4
  fprintf('    ');
  for c = 1:4; fprintf('%2d ', B(r,c)); end
  fprintf('\n');
end

% Salt-and-pepper-style: ones with one outlier
A2 = uint8(ones(5));
A2(3,3) = 7;
B = modefilt(A2, [3 3]);
fprintf('\n  salt-and-pepper recovery: B(3,3)=%d  (e 1)\n', B(3,3));

% uint16 input
A3 = uint16(reshape([repmat(1000, 1, 8), repmat(2000, 1, 8)], 4, 4));
B = modefilt(A3, [3 3]);
fprintf('  uint16 class preserved: %s\n', class(B));

fprintf('\nTie-break: smallest-wins (matches base mode). Bit-equal\n');
fprintf('MATLAB R2025b on probed interior fingerprints.\n');

clear

% imrotate3 — 3-D volumetric rotation smoke.
% Reference engine: MATLAB R2025b Image Processing Toolbox.

A = reshape(double(1:27), 3, 3, 3);

fprintf('== axis-aligned 90° (no interpolation) ==\n');
B = imrotate3(A, 90, [0 0 1]);
fprintf('  z90 size=[%d %d %d]   B(1,1,1)=%.6f (expect 7)\n', size(B,1), size(B,2), size(B,3), B(1,1,1));
B = imrotate3(A, 90, [1 0 0]);
fprintf('  x90 B(1,1,1)=%.6f (expect 3)\n', B(1,1,1));
B = imrotate3(A, 90, [0 1 0]);
fprintf('  y90 B(1,1,1)=%.6f (expect 19)\n', B(1,1,1));

fprintf('\n== 45° z (loose, linear) ==\n');
B = imrotate3(A, 45, [0 0 1]);
fprintf('  size=[%d %d %d] (expect 5 5 3)   B(3,3,2)=%.6f (expect 14)   B(3,3,3)=%.6f (expect 23)\n', ...
        size(B,1), size(B,2), size(B,3), B(3,3,2), B(3,3,3));

fprintf('\n== 45° z (crop) ==\n');
B = imrotate3(A, 45, [0 0 1], 'linear', 'crop');
fprintf('  size=[%d %d %d] (expect 3 3 3)   B(2,2,2)=%.6f (expect 14)\n', ...
        size(B,1), size(B,2), size(B,3), B(2,2,2));

fprintf('\n== oblique 60° around [1 1 1] ==\n');
B = imrotate3(A, 60, [1 1 1]);
fprintf('  size=[%d %d %d] (expect 5 5 5)   B(3,3,3)=%.6f (expect 14)\n', ...
        size(B,1), size(B,2), size(B,3), B(3,3,3));

fprintf('\n== methods ==\n');
B = imrotate3(A, 30, [0 0 1], 'nearest');
fprintf('  nearest 30° z   B(3,3,2)=%.6f (expect 14)\n', B(3,3,2));
B = imrotate3(A, 30, [0 0 1], 'cubic');
fprintf('  cubic 30° z     B(3,3,2)=%.6f (expect 14)\n', B(3,3,2));

fprintf('\n== identity (angle=0) ==\n');
B = imrotate3(A, 0, [0 0 1]);
fprintf('  B(1,1,1)=%.6f (expect 1)   B(2,2,2)=%.6f (expect 14)\n', B(1,1,1), B(2,2,2));

fprintf('\n== FillValues ==\n');
B = imrotate3(A, 45, [0 0 1], 'linear', 'loose', 'FillValues', -99);
fprintf('  B(1,1,1)=%.6f (expect -99)\n', B(1,1,1));

fprintf('\n== uint8 preserved ==\n');
A8 = uint8(reshape(1:27, 3, 3, 3));
B = imrotate3(A8, 90, [0 0 1]);
fprintf('  class=%s   B(1,1,1)=%d (expect 7)   B(3,3,3)=%d (expect 21)\n', class(B), B(1,1,1), B(3,3,3));

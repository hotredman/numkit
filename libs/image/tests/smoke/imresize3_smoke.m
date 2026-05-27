clear
import compat.*

% imresize3 — 3-D volumetric resampling smoke.
% Reference engine: MATLAB R2025b Image Processing Toolbox.

A = reshape(double(1:60), 3, 4, 5);

fprintf('== scale=2 cubic (default) ==\n');
B = imresize3(A, 2);
fprintf('  size=[%d %d %d]   B(1,1,1)=%.6f (expect -0.5)\n', size(B,1), size(B,2), size(B,3), B(1,1,1));
fprintf('  B(2,2,2)=%.6f (expect 3.875)   B(3,3,3)=%.6f (expect 12.625)\n', B(2,2,2), B(3,3,3));
fprintf('  B(6,8,10)=%.6f (expect 61.5)\n', B(6,8,10));

fprintf('\n== nearest ==\n');
B = imresize3(A, 2, 'nearest');
fprintf('  B(1,1,1)=%d (expect 1)   B(3,3,3)=%d (expect 17)\n', B(1,1,1), B(3,3,3));

fprintf('\n== linear ==\n');
B = imresize3(A, 2, 'linear');
fprintf('  B(1,1,1)=%.6f (expect 1)   B(3,3,3)=%.6f (expect 13)   B(6,8,10)=%.6f (expect 60)\n', B(1,1,1), B(3,3,3), B(6,8,10));

fprintf('\n== shrink 0.5 cubic+AA ==\n');
B = imresize3(A, 0.5);
fprintf('  B(1,1,1)=%.6f (expect 8.292969)   B(end,end,end)=%.6f (expect 58.390625)\n', B(1,1,1), B(2,2,3));

fprintf('\n== shrink 0.5 linear ==\n');
B = imresize3(A, 0.5, 'linear');
fprintf('  B(1,1,1)=%.6f (expect 11)   B(end,end,end)=%.6f (expect 54.875)\n', B(1,1,1), B(2,2,3));

fprintf('\n== shrink 0.5 box ==\n');
B = imresize3(A, 0.5, 'box');
fprintf('  B(1,1,1)=%.6f (expect 9)   B(end,end,end)=%.6f (expect 58.5)\n', B(1,1,1), B(2,2,3));

fprintf('\n== shrink 0.5 cubic, Antialiasing=false ==\n');
B = imresize3(A, 0.5, 'cubic', 'Antialiasing', false);
fprintf('  B(1,1,1)=%.6f (expect 8)   B(end,end,end)=%.6f (expect 60.3125)\n', B(1,1,1), B(2,2,3));

fprintf('\n== size [2 2 3] (uses out/in scale, differs from 0.5) ==\n');
B = imresize3(A, [2 2 3]);
fprintf('  B(1,1,1)=%.6f (expect 6.101814)\n', B(1,1,1));

fprintf('\n== Lanczos kernels ==\n');
B = imresize3(A, 2, 'lanczos2');
fprintf('  lanczos2 B(3,3,3)=%.6f (expect 12.213543)\n', B(3,3,3));
B = imresize3(A, 2, 'lanczos3');
fprintf('  lanczos3 B(3,3,3)=%.6f (expect 12.552874)\n', B(3,3,3));

fprintf('\n== uint8 preserved ==\n');
A8 = uint8(reshape(1:60, 3, 4, 5));
B = imresize3(A8, 2);
fprintf('  class=%s   B(1,1,1)=%d (expect 0)   B(3,3,3)=%d (expect 13)\n', class(B), B(1,1,1), B(3,3,3));

fprintf('\n== NV pairs ==\n');
B = imresize3(A, 'Scale', 0.5);
fprintf('  Scale=0.5 NV   B(1,1,1)=%.6f (expect 8.292969)\n', B(1,1,1));
B = imresize3(A, 'OutputSize', [2 2 3]);
fprintf('  OutputSize=[2 2 3] NV   B(1,1,1)=%.6f (expect 6.101814)\n', B(1,1,1));

clear
import compat.*

% fibermetric — Frangi 1998 vesselness filter.
% Reference values from MATLAB R2025b probe at tmp/fm_probe.m.

A = zeros(20, 20, 'uint8');
A(:, 10:11) = 200;        % vertical bright tube
A(10:11, :) = 200;        % horizontal bright tube
A(8, 10) = 200; A(13, 10) = 200;

fprintf('=== Default ===\n');
B = fibermetric(A);
fprintf('class=%s size=[%d %d]\n', class(B), size(B,1), size(B,2));
fprintf('B(5,10)=%.6f (expect ~1.0)  B(10,5)=%.6f (expect ~1.0)\n', B(5,10), B(10,5));
fprintf('B(1,1)=%.6f (expect 0)\n', B(1,1));

fprintf('\n=== thickness=4 ===\n');
B = fibermetric(A, 4);
fprintf('B(5,10)=%.6f (expect 1.0)\n', B(5,10));

fprintf('\n=== dark polarity on inverted ===\n');
Ainv = uint8(255 - A);
B = fibermetric(Ainv, 4, 'ObjectPolarity', 'dark');
fprintf('B(5,10)=%.6f (expect 1.0 — symmetric with bright case)\n', B(5,10));

fprintf('\n=== StructureSensitivity=50 (high c → reduced response) ===\n');
B = fibermetric(A, 4, 'StructureSensitivity', 50);
fprintf('B(5,10)=%.6f (expect < 1.0)\n', B(5,10));

fprintf('\n=== single input ===\n');
As = single(A)/255;
B = fibermetric(As);
fprintf('class=%s  B(5,10)=%.6f\n', class(B), B(5,10));

fprintf('\n=== double input ===\n');
Ad = double(A)/255;
B = fibermetric(Ad);
fprintf('class=%s  B(5,10)=%.6f\n', class(B), B(5,10));

fprintf('\n=== Flat ===\n');
F = uint8(100*ones(20,20));
B = fibermetric(F);
fprintf('B(10,10)=%.6f (expect 0)  max(B)=%.6f\n', B(10,10), max(B(:)));

fprintf('\n=== 3-D ===\n');
V = zeros(20, 20, 20, 'uint8');
V(:, 10:11, 10:11) = 200;
B = fibermetric(V, 4);
fprintf('class=%s size=[%d %d %d]  B(10,10,10)=%.6f\n', class(B), size(B,1), size(B,2), size(B,3), B(10,10,10));

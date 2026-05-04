import compat.*

% --- imsplit on a small RGB volume ---
% Build I (3x4x3) where each plane has a distinct constant added to the
% pixel index, so we can verify the planes are returned in order.
H = 3; W = 4;
basis = reshape(1:(H*W), [H W]);
I = zeros(H, W, 3);
I(:, :, 1) = basis;            % plane 1
I(:, :, 2) = basis + 100;      % plane 2
I(:, :, 3) = basis + 200;      % plane 3

[R, G, B] = imsplit(I);

fprintf('--- imsplit on 3x4x3 volume ---\n');
fprintf('  size(R) = [%d %d] (expect [3 4])\n', size(R, 1), size(R, 2));
fprintf('  R(2,3) = %.1f (expect %.1f)\n', R(2, 3), basis(2, 3));
fprintf('  G(2,3) = %.1f (expect %.1f)\n', G(2, 3), basis(2, 3) + 100);
fprintf('  B(2,3) = %.1f (expect %.1f)\n', B(2, 3), basis(2, 3) + 200);
fprintf('  max|R - basis|       = %.6e\n', max(max(abs(R - basis))));
fprintf('  max|G - basis - 100| = %.6e\n', max(max(abs(G - basis - 100))));
fprintf('  max|B - basis - 200| = %.6e\n\n', max(max(abs(B - basis - 200))));

% --- 2D image: imsplit returns a single plane equal to input ---
I2 = magic_like(5);  % helper below; just any 5x5 deterministic
P = imsplit(I2);
fprintf('--- imsplit on 2D input ---\n');
fprintf('  max|P - I2| = %.6e (expect 0)\n\n', max(max(abs(P - I2))));

% --- imhistmatch: scaled image should be remapped to reference scale ---
% Source image values in [0, 0.5]; reference values in [0, 1].
% imhistmatch should stretch the source to fill [0, 1].
src = (0 : 0.01 : 0.5)';     % 51 values
ref = (0 : 0.01 : 1.0)';     % 101 values

J = imhistmatch(src, ref, 64);
fprintf('--- imhistmatch(src in [0,.5], ref in [0,1]) ---\n');
fprintf('  min(src) = %.4f, max(src) = %.4f\n', min(src), max(src));
fprintf('  min(J)   = %.4f, max(J)   = %.4f (expect spread close to [0, 1])\n\n', ...
    min(J), max(J));

% --- Identity: histmatching to itself is (approx) identity ---
I3 = (0 : 0.01 : 1)';
J3 = imhistmatch(I3, I3, 64);
% Small bin discretisation residual is expected; check < 1/64 = 0.0156
err3 = max(abs(J3 - I3));
fprintf('--- imhistmatch(I, I) — near-identity ---\n');
fprintf('  max|J - I| = %.4f (expect < 1/64 = 0.0156)\n', err3);

function M = magic_like(n)
    M = mod((1:(n*n))' - 1, 7);
    M = reshape(M, [n n]);
end

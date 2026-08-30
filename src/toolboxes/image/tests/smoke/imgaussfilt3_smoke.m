clear

% imgaussfilt3 — 3-D Gaussian filter, separable, replicate boundary.

fprintf('--- ones(3,3,3) → all ~1 ---\n');
V = ones(3,3,3);
J = imgaussfilt3(V, 1.5);
fprintf('all close to 1? %d\n', all(abs(J(:) - 1) < 1e-12));

fprintf('\n--- impulse 5x5x5 with σ=1 ---\n');
V = zeros(5,5,5);
V(3,3,3) = 1;
J = imgaussfilt3(V, 1);
fprintf('center pixel: %.6f\n', J(3,3,3));
fprintf('mass conserved (sum): %.6f  (expect 1)\n', sum(J(:)));

fprintf('\n--- per-axis sigma [0.5 1 2] ---\n');
V = zeros(7,7,7);
V(4,4,4) = 1;
J = imgaussfilt3(V, [0.5 1 2]);
fprintf('mass conserved: %.6f  (expect 1)\n', sum(J(:)));
fprintf('size: %s\n', mat2str(size(J)));

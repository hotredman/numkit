clear

% imboxfilt3 — separable 3-D mean filter, replicate boundary, default 3.

% --- 3x3x3 ones volume → mean is 1 everywhere ---
fprintf('--- imboxfilt3(ones(3,3,3)) ---\n');
V = ones(3,3,3);
J = imboxfilt3(V);
fprintf('size: %s\n', mat2str(size(J)));
fprintf('all close to 1? %d\n', all(abs(J(:) - 1) < 1e-12));

% --- impulse 5x5x5 with 3 box → averages over 27 nbhd ---
fprintf('\n--- delta impulse, FilterSize=3 ---\n');
V = zeros(5,5,5);
V(3,3,3) = 27;
J = imboxfilt3(V, 3);
fprintf('center pixel (was 27): %.4f  (expect 1)\n', J(3,3,3));
fprintf('corner of nbhd (2,2,2): %.4f  (expect 1)\n', J(2,2,2));
fprintf('outside nbhd (1,1,1): %.4f  (expect 0)\n', J(1,1,1));
fprintf('mean over volume: %.6f  (expect 1*27/125 = 0.216)\n', mean(J(:)));

% --- non-cube filter [3 5 1] ---
fprintf('\n--- FilterSize=[3 5 1] ---\n');
V = ones(7,7,3);
J = imboxfilt3(V, [3 5 1]);
fprintf('all close to 1? %d (replicate boundary preserves 1)\n', all(abs(J(:) - 1) < 1e-12));

% --- uint8 input → uint8 output ---
fprintf('\n--- uint8 volume ---\n');
V = uint8(50 * ones(4,4,4));
J = imboxfilt3(V, 3);
fprintf('class: %s\n', class(J));
fprintf('all = 50? %d\n', all(double(J(:)) == 50));

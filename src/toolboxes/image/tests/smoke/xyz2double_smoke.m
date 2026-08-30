clear

% xyz2double — XYZ uint16/double → double, ICC encoding.

fprintf('--- uint16 [0, 32768, 65535] ---\n');
d = xyz2double(uint16([0 32768 65535]));
disp(d);
fprintf('  expect [0, 1, 1.999969] (= 1 + 32767/32768)\n\n');

fprintf('--- double passthrough ---\n');
disp(xyz2double([0.5 1.0 0.25]));
fprintf('  expect [0.5 1.0 0.25]\n\n');

fprintf('--- M-by-3 colormap (uint16) ---\n');
M = uint16([0 32768 32768; 16384 32768 0]);
disp(xyz2double(M));
fprintf('  expect [0 1 1; 0.5 1 0]\n\n');

fprintf('--- 2-by-2-by-3 image (uint16) ---\n');
I = uint16(cat(3, [0 32768; 32768 0], ...
                 [32768 0; 0 32768], ...
                 [0 32768; 32768 0]));
J = xyz2double(I);
fprintf('size: %s\n', mat2str(size(J)));
fprintf('J(1,1,:) = [%.4f %.4f %.4f]\n', J(1,1,1), J(1,1,2), J(1,1,3));
fprintf('J(2,2,:) = [%.4f %.4f %.4f]\n', J(2,2,1), J(2,2,2), J(2,2,3));

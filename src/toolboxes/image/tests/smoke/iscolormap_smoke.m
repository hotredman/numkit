clear

% iscolormap — validate N×3 float colormap.

fprintf('--- valid 4x3 double ---\n');
M = [0 0 1; 0 1 0; 1 0 0; 1 1 0];
fprintf('iscolormap = %d (expect 1)\n', iscolormap(M));

fprintf('\n--- valid 1x3 (single row) ---\n');
fprintf('iscolormap = %d (expect 1)\n', iscolormap([0.5 0.5 0.5]));

fprintf('\n--- empty 0x3 ---\n');
fprintf('iscolormap(zeros(0,3)) = %d (expect 0)\n', iscolormap(zeros(0,3)));

fprintf('\n--- 4 columns ---\n');
fprintf('iscolormap(zeros(3,4)) = %d (expect 0)\n', iscolormap(zeros(3,4)));

fprintf('\n--- 3-D ---\n');
fprintf('iscolormap(ones(3,3,3)) = %d (expect 0)\n', iscolormap(ones(3,3,3)));

fprintf('\n--- uint8 (not float) ---\n');
fprintf('iscolormap(uint8([0 128 255])) = %d (expect 0)\n', ...
        iscolormap(uint8([0 128 255])));

fprintf('\n--- single class is OK ---\n');
fprintf('iscolormap(single([0.1 0.2 0.3])) = %d (expect 1)\n', ...
        iscolormap(single([0.1 0.2 0.3])));

fprintf('\n--- complex rejected ---\n');
fprintf('iscolormap([0 1i 0]) = %d (expect 0)\n', iscolormap([0 1i 0]));

fprintf('\n--- out-of-range NOT enforced (Octave behaviour) ---\n');
fprintf('iscolormap([-1 2 3]) = %d (expect 1)\n', iscolormap([-1 2 3]));

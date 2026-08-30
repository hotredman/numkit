clear

% colorangle — angle in degrees between two RGB colours.

fprintf('--- Octave-source reference vectors ---\n');
fprintf('colorangle([1 1 1], [1 1 1])   = %.4f (expect 0)\n', ...
        colorangle([1 1 1], [1 1 1]));
fprintf('colorangle([0 0 0], [0 0 0])   = %.4f (expect 0)\n', ...
        colorangle([0 0 0], [0 0 0]));
fprintf('colorangle([0 0 0], [0 1 0])   = %s (expect NaN)\n', ...
        num2str(colorangle([0 0 0], [0 1 0])));
fprintf('colorangle([1 0 0], [-1 0 0])  = %.4f (expect 180)\n', ...
        colorangle([1 0 0], [-1 0 0]));
fprintf('colorangle([0 0 1], [1 0 0])   = %.4f (expect 90)\n', ...
        colorangle([0 0 1], [1 0 0]));
fprintf('colorangle([0;0;1], [1 0 0])   = %.4f (expect 90; col-vec accepted)\n', ...
        colorangle([0;0;1], [1 0 0]));

fprintf('\n--- broadcast: N×3 vs 3-vec ---\n');
a = [1 0 0; 0.5 1 0; 0 1 1; 1 1 1];
b = [0 1 0];
ang = colorangle(a, b);
fprintf('size = %s, values:\n', mat2str(size(ang)));
disp(ang);

fprintf('\n--- 0.5,0.61237,-0.61237 vs 0.86603,0.35355,-0.35355 ---\n');
fprintf('expect ~30: %.6f\n', ...
        colorangle([0.5 0.61237 -0.61237], [0.86603 0.35355 -0.35355]));

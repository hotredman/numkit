clear

import compat.*

% Step image — vertical edge in the middle column
A = [0 0 0 9 9 9;
     0 0 0 9 9 9;
     0 0 0 9 9 9];

[Gx, Gy] = imgradientxy(A);
fprintf('--- imgradientxy(step) Gx ---\n');
disp(Gx);
fprintf('  expect: large POSITIVE Gx at the step (intensity rises left->right; MATLAB sign convention)\n\n');

[Gmag, Gdir] = imgradient(A);
fprintf('--- imgradient magnitude ---\n');
disp(Gmag);
fprintf('  expect: peaks at column 2-3 (the step) ≈ 36\n\n');

fprintf('--- edge(A, ''sobel'') ---\n');
disp(double(edge(A, 'sobel')));
fprintf('  expect: 1s along the step\n\n');

fprintf('--- edge(A, ''prewitt'') ---\n');
disp(double(edge(A, 'prewitt')));
fprintf('  expect: similar to sobel\n\n');

fprintf('--- edge(A, ''roberts'') ---\n');
disp(double(edge(A, 'roberts')));
fprintf('  expect: 2-pixel-wide edge response\n\n');

% Larger smoothly-varying image
B = [1 2 3 4 5 6;
     2 3 4 5 6 7;
     3 4 5 6 7 8;
     4 5 6 7 8 9;
     5 6 7 8 9 10];
fprintf('--- imgradient on linear ramp ---\n');
[Gm, ~] = imgradient(B);
disp(Gm);
fprintf('  expect: ~uniform interior values, magnitude ≈ |∇|\n');

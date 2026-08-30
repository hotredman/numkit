clear

% imdiffusefilt — Perona-Malik anisotropic diffusion (1990).

I = double([1 2 3 4 5; 6 7 8 9 10; 11 12 13 14 15;
            16 17 18 19 20; 21 22 23 24 25]) / 25;

fprintf('=== default (N=5, maximal, exponential, K=0.1 for double) ===\n');
B = imdiffusefilt(I);
fprintf('B(3,3) = %.6f (expect 0.520000)\n', B(3,3));
fprintf('B(1,1) = %.8f (expect 0.07067272)\n', B(1,1));
fprintf('B(5,5) = %.8f (expect 0.96932728)\n', B(5,5));

fprintf('\n=== N = 3 ===\n');
B = imdiffusefilt(I, 'NumberOfIterations', 3);
fprintf('B(3,3) = %.6f (expect 0.520000)\n', B(3,3));

fprintf('\n=== Connectivity = minimal (4-conn) ===\n');
B = imdiffusefilt(I, 'Connectivity', 'minimal');
fprintf('B(3,3) = %.6f (expect 0.520000)\n', B(3,3));

fprintf('\n=== ConductionMethod = quadratic ===\n');
B = imdiffusefilt(I, 'ConductionMethod', 'quadratic');
fprintf('B(3,3) = %.6f (expect 0.520000)\n', B(3,3));

fprintf('\n=== custom GradientThreshold scalar = 0.5 ===\n');
B = imdiffusefilt(I, 'GradientThreshold', 0.5);
fprintf('B(3,3) = %.6f (expect 0.520000)\n', B(3,3));

fprintf('\n=== vector GradientThreshold (N inferred = 3) ===\n');
B = imdiffusefilt(I, 'GradientThreshold', [0.1 0.2 0.3]);
fprintf('B(3,3) = %.6f (expect 0.520000)\n', B(3,3));

fprintf('\n=== uint8 input class ===\n');
Iu = uint8(I*255);
B = imdiffusefilt(Iu);
fprintf('B(3,3) = %d (expect 133) class=%s\n', B(3,3), class(B));

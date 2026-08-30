clear

fprintf('=== fspecial3 (3-D image filter kernels) ===\n');

h = fspecial3('average');
fprintf('average default: size=[%d %d %d] sum=%g (expect 5 5 5, 1)\n', ...
        size(h,1), size(h,2), size(h,3), sum(h(:)));
h = fspecial3('average', [3 5 7]);
fprintf('  [3 5 7]: size=[%d %d %d] val=%.8g (expect 3 5 7, 0.0095238)\n', ...
        size(h,1), size(h,2), size(h,3), h(1,1,1));

h = fspecial3('gaussian');
fprintf('gaussian default: sum=%g center=%.8g (expect 1, 0.065266)\n', sum(h(:)), h(3,3,3));
h = fspecial3('gaussian', [3 3 3], [0.5 5 5]);
fprintf('  anisotropic [0.5 5 5]: center=%.8g corner=%.8g (expect 0.0897981, 0.0116763)\n', ...
        h(2,2,2), h(1,1,1));

h = fspecial3('ellipsoid');
fprintf('ellipsoid default: size=[%d %d %d] nnz=%d val=%.8g (expect 11 11 11, 515, 0.0019417)\n', ...
        size(h,1), size(h,2), size(h,3), nnz(h), h(6,6,6));
h = fspecial3('ellipsoid', [2 3 4]);
fprintf('  [2 3 4]: size=[%d %d %d] (expect 5 7 9)\n', size(h,1), size(h,2), size(h,3));

h = fspecial3('laplacian');
fprintf('laplacian default: size=[%d %d %d] sum=%g center=%g (expect 3 3 3, 0, -6)\n', ...
        size(h,1), size(h,2), size(h,3), sum(h(:)), h(2,2,2));
h = fspecial3('laplacian', 0.2, 0.3);
fprintf('  (0.2,0.3): center=%g face=%g edge=%g corner=%g (expect -4.2, 0.5, 0.05, 0.075)\n', ...
        h(2,2,2), h(2,2,1), h(1,2,1), h(1,1,1));

h = fspecial3('log');
fprintf('log default: sum=%.2g center=%.8g corner=%.8g (expect ~0, -0.193981, 0.0032725)\n', ...
        sum(h(:)), h(3,3,3), h(1,1,1));
h = fspecial3('log', [5 5 5], [1 1.5 2]);
fprintf('  anisotropic [1 1.5 2]: corner=%.8g center=%.8g (expect 0.0064098, -0.0470185)\n', ...
        h(1,1,1), h(3,3,3));

h = fspecial3('sobel', 'X');
fprintf('sobel X: h(1,1,1)=%g h(1,1,2)=%g h(2,2,2)=%g (expect 1, 2, 0)\n', ...
        h(1,1,1), h(1,1,2), h(2,2,2));
h = fspecial3('prewitt', 'Z');
fprintf('prewitt Z: h(1,1,1)=%g h(1,1,3)=%g (expect 1, -1)\n', h(1,1,1), h(1,1,3));

fprintf('\n=== fwind2 (2-D FIR via 2-D window) ===\n');
h = fwind2(ones(7), ones(7));
fprintf('fwind2(ones(7), ones(7)): size=[%d %d] sum=%g (expect 1)\n', ...
        size(h,1), size(h,2), sum(h(:)));

fprintf('\n=== Deferred (MATLAB-OOP, blocked by section 0) ===\n');
try; gabor(4, 0); catch e; fprintf('gabor: %s\n', strtok(e.message, char(10))); end

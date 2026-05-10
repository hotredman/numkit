clear
import compat.*

fprintf('=== fspecial3 (3-D image filter kernels) ===\n');

h = fspecial3('average');
fprintf('average default: size=[%d %d %d] sum=%g (expect 1)\n', ...
        size(h,1), size(h,2), size(h,3), sum(h(:)));

h = fspecial3('gaussian');
fprintf('gaussian default: sum=%g (expect 1)\n', sum(h(:)));
h = fspecial3('gaussian', [3 3 3], 1);
fprintf('  3x3x3 sigma=1: center=%g (expect 0.092261)\n', h(2,2,2));

h = fspecial3('laplacian');
fprintf('laplacian default: size=[%d %d %d] sum=%g (expect 0)\n', ...
        size(h,1), size(h,2), size(h,3), sum(h(:)));
fprintf('  center=%g (expect -6)\n', h(2,2,2));

h = fspecial3('sobel', 'X');
fprintf('sobel X: h(1,1,1)=%g h(1,1,2)=%g h(2,2,2)=%g (expect 1, 2, 0)\n', ...
        h(1,1,1), h(1,1,2), h(2,2,2));

h = fspecial3('ellipsoid');
fprintf('ellipsoid default: sum=%g (expect 1)\n', sum(h(:)));

fprintf('\n=== fwind2 (2-D FIR via 2-D window) ===\n');
Hd = ones(7);
w = ones(7);
h = fwind2(Hd, w);
fprintf('fwind2(ones(7), ones(7)): size=[%d %d] sum=%g (expect 1)\n', ...
        size(h,1), size(h,2), sum(h(:)));

fprintf('\n=== KNOWN GAPs (deferred to v2) ===\n');
try; fsamp2(ones(7)); catch e; fprintf('fsamp2: %s\n', strtok(e.message, char(10))); end
try; ftrans2([1 2 1]/4); catch e; fprintf('ftrans2: %s\n', strtok(e.message, char(10))); end
try; fwind1(ones(7), [1 1 1 1 1 1 1]); catch e; fprintf('fwind1: %s\n', strtok(e.message, char(10))); end
try; gabor(4, 0); catch e; fprintf('gabor: %s\n', strtok(e.message, char(10))); end

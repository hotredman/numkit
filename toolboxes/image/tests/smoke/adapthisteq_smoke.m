clear
import compat.*

fprintf('=== image/adapthisteq — Contrast Limited Adaptive HistEq (CLAHE) ===\n');
fprintf('Clean-room CLAHE: Zuiderveld 1994; Pizer et al. 1990 / 1987.\n');

% Smooth gradient — classic CLAHE test image.
[X, Y] = meshgrid(linspace(0, 1, 64), linspace(0, 1, 64));
I = uint8(255 * sqrt(X.*Y));

fprintf('\n[default args: NumTiles=[8 8], ClipLimit=0.01, NBins=256]\n');
J = adapthisteq(I);
fprintf('  class(J) = %s  size = [%d %d]   (expect uint8 [64 64])\n', ...
    class(J), size(J, 1), size(J, 2));
fprintf('  J(1,1) = %d   J(end,end) = %d   (corners clamp to 0 / 255)\n', ...
    J(1, 1), J(end, end));
fprintf('  range [%d, %d]   mean = %.2f\n', ...
    min(J(:)), max(J(:)), mean(double(J(:))));

fprintf('\n[coarse tiles: NumTiles=[4 4]]\n');
J4 = adapthisteq(I, 'NumTiles', [4 4]);
fprintf('  J4(32,32) = %d   (interior pixel after 4x4-tile transform)\n', ...
    J4(32, 32));

fprintf('\n[no-clip: ClipLimit=1.0 -> per-tile histeq + bilinear blend]\n');
Jnc = adapthisteq(I, 'ClipLimit', 1.0);
fprintf('  Jnc range [%d, %d]   mean = %.2f\n', ...
    min(Jnc(:)), max(Jnc(:)), mean(double(Jnc(:))));

fprintf('\n[Distribution: rayleigh / exponential, Alpha=0.4]\n');
R = adapthisteq(I, 'Distribution', 'rayleigh');
E = adapthisteq(I, 'Distribution', 'exponential');
fprintf('  rayleigh    range [%d, %d]   mean = %.2f\n', ...
    min(R(:)), max(R(:)), mean(double(R(:))));
fprintf('  exponential range [%d, %d]   mean = %.2f\n', ...
    min(E(:)), max(E(:)), mean(double(E(:))));

fprintf('\n[Range: full vs original]\n');
lo = uint8(120 + 16 * sqrt(X.*Y));   % low-contrast: 16-level band
hf = adapthisteq(lo, 'ClipLimit', 1.0, 'Range', 'full');
ro = adapthisteq(lo, 'ClipLimit', 1.0, 'Range', 'original');
fprintf('  input  range [%d, %d]   std = %.3f\n', ...
    min(lo(:)), max(lo(:)), std(double(lo(:))));
fprintf('  full     -> range [%d, %d]   std = %.3f   (expect wide spread)\n', ...
    min(hf(:)), max(hf(:)), std(double(hf(:))));
fprintf('  original -> range [%d, %d]   (expect stays within input min/max)\n', ...
    min(ro(:)), max(ro(:)));

fprintf('\nDefining property of CLAHE: a low-contrast image gains a much\n');
fprintf('wider dynamic range. Clean-room impl is functionally equivalent\n');
fprintf('to MATLAB R2025b adapthisteq, not bit-identical — interior\n');
fprintf('pixels differ by undocumented MATLAB clip/interp rounding.\n');
fprintf('Octave 11.1.0 does not ship adapthisteq in its image package.\n');

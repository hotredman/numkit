clear
import compat.*

fprintf('=== image/adapthisteq — Contrast Limited Adaptive HistEq (CLAHE) ===\n');

% Smooth gradient — classic CLAHE test image.
[X, Y] = meshgrid(linspace(0, 1, 64), linspace(0, 1, 64));
I = uint8(255 * sqrt(X.*Y));

fprintf('\n[default args: NumTiles=[8 8], ClipLimit=0.01, NBins=256]\n');
J = adapthisteq(I);
fprintf('  class(J) = %s  size = [%d %d]\n', class(J), size(J, 1), size(J, 2));
fprintf('  J(1,1) = %d         (top-left near min of remapped range)\n', J(1, 1));
fprintf('  J(end,end) = %d   (bottom-right saturates at 255)\n', J(end, end));
fprintf('  range [%d, %d]   mean = %.2f\n', min(J(:)), max(J(:)), mean(double(J(:))));

fprintf('\n[coarse tiles: NumTiles=[4 4]]\n');
J4 = adapthisteq(I, 'NumTiles', [4 4]);
fprintf('  J4(32,32) = %d  (interior pixel after 4x4-tile transform)\n', J4(32, 32));

fprintf('\n[no-clip: ClipLimit=1.0 -> plain per-tile histeq + bilinear blend]\n');
Jnc = adapthisteq(I, 'ClipLimit', 1.0);
fprintf('  Jnc range [%d, %d]   mean = %.2f\n', ...
    min(Jnc(:)), max(Jnc(:)), mean(double(Jnc(:))));

fprintf('\nApprox-equal MATLAB R2025b at corners / saturation; interior may\n');
fprintf('shift ~10-50 units on uint8 (MATLAB uses single-tile in outer\n');
fprintf('half of corner tiles; this impl bilinear-interpolates everywhere\n');
fprintf('— documented GAP). Octave 11.1.0 doesn''t ship adapthisteq in\n');
fprintf('the indexed image package.\n');

clear
import compat.*

% graythresh — global Otsu threshold, normalised to [0,1]. DEEP-PROBE c182.
% numkit built the histogram with default_nbins (64 bins for floating-point,
% 65536 for uint16), but MATLAB graythresh ALWAYS uses NPTS=256 bins. So the
% level was off for float/uint16 inputs (double in [0,1] gave 0.507937 vs
% MATLAB 0.513725). Now uses 256 bins like MATLAB. uint8 (already 256) is
% unchanged. All values pinned to MATLAB R2025b.

fprintf('--- uint8 bimodal (unchanged, already 256 bins) ---\n');
Iu = uint8([20 20 20 20 120 120 120 120 220 220 220 220]);
fprintf('level = %.6f   (expect 0.468627)\n', graythresh(Iu));

fprintf('\n--- double in [0,1] (64 -> 256 bins) ---\n');
B = [0.1 0.2 0.3 0.8 0.9; 0.15 0.25 0.85 0.95 0.05];
[lb, eb] = graythresh(B);
fprintf('level = %.6f   (expect 0.549020)\n', lb);
fprintf('EM    = %.6f   (expect 0.954264)\n', eb);

fprintf('\n--- uint16 (65536 -> 256 bins) ---\n');
Cu = uint16([1000 2000 30000 40000 50000 60000 5000 8000]);
fprintf('level = %.6f   (expect 0.288235)\n', graythresh(Cu));

fprintf('\n--- graythresh feeds imbinarize ''global'' ---\n');
bw = imbinarize(B);
fprintf('imbinarize(B) foreground count = %d (B > level)\n', sum(bw(:)));

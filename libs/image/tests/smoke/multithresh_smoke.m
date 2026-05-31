clear
import compat.*

% multithresh — multi-level Otsu thresholds. DEEP-PROBE c181 REWRITE.
% numkit previously binned floating-point data over [0,1] (via imhist) and
% returned normalised / midpoint-of-means thresholds, so non-[0,1] data was
% wildly wrong (multithresh(reshape(1:36,6,6)) gave 0.49 vs MATLAB 18.43).
% Now follows MATLAB's getpdf (scale to [0,1] by the data range -> 256-bin
% grayto8 histogram) + Otsu objective + map2OriginalScale, so thresholds are
% returned in the input's value range. N=1/N=2 are bit-exact vs MATLAB R2025b.

fprintf('--- double data 1..36 (thresholds in data range) ---\n');
A = reshape(1:36, 6, 6);
fprintf('N=1: %.6f            (expect 18.431373)\n', multithresh(A));
t2 = multithresh(A, 2);
fprintf('N=2: %.6f %.6f  (expect 12.392157 24.470588)\n', t2(1), t2(2));
[~, em2] = multithresh(A, 2);
[~, em1] = multithresh(A, 1);
fprintf('metric N=1=%.6f N=2=%.6f (expect 0.750205 0.889321)\n', em1, em2);

fprintf('\n--- double in [0,1] ---\n');
Ad = (1:36) / 36;
td = multithresh(Ad, 2);
fprintf('N=2: %.6f %.6f  (expect 0.344227 0.679739)\n', td(1), td(2));

fprintf('\n--- uint8 input (rounded back to the integer range) ---\n');
Au = uint8([10 50 90 130 170 210 250 30 70]);
fprintf('N=1: %d        (expect 110)\n', double(multithresh(Au, 1)));
tu = multithresh(Au, 2);
fprintf('N=2: %d %d     (expect 110 190)\n', double(tu(1)), double(tu(2)));

fprintf('\n--- three separated clusters (20/120/220) ---\n');
Is = uint8([repmat(20,1,100), repmat(120,1,100), repmat(220,1,100)]);
ts = multithresh(Is, 2);
fprintf('N=2: %d %d     (expect 70 170, sum 240)\n', double(ts(1)), double(ts(2)));

fprintf('\n--- N>=3 uses a global DP (may differ from MATLAB fminsearch) ---\n');
t3 = multithresh(A, 3);
fprintf('N=3: %.4f %.4f %.4f (in data range, monotonic)\n', t3(1), t3(2), t3(3));

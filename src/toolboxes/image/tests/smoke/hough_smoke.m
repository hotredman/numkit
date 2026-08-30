clear

% hough + houghpeaks — Standard Hough Transform for line detection.
% Bit-exact MATLAB R2025b.

BW = false(11, 11);
BW(6, 1:11) = true;
BW(1:11, 6) = true;

fprintf('=== default Hough ===\n');
[H, T, R] = hough(BW);
fprintf('size(H) = [%d %d] (expect [31 180])\n', size(H,1), size(H,2));
fprintf('T = [%g .. %g]  R = [%g .. %g]\n', T(1), T(end), R(1), R(end));
fprintf('max(H(:)) = %d (expect 11)\n', max(H(:)));

fprintf('\n=== custom RhoResolution = 2 ===\n');
[H2, T2, R2] = hough(BW, 'RhoResolution', 2);
fprintf('size(H2) = [%d %d] (expect [17 180]) max=%d\n', size(H2,1), size(H2,2), max(H2(:)));

fprintf('\n=== custom Theta = -45:45 ===\n');
[H3, T3, R3] = hough(BW, 'Theta', -45:45);
fprintf('size(H3) = [%d %d] (expect [31 91]) max=%d\n', size(H3,1), size(H3,2), max(H3(:)));

fprintf('\n=== houghpeaks default ===\n');
P = houghpeaks(H, 2);
fprintf('P = [%d %d; %d %d] (expect [11 1; 21 89])\n', ...
    P(1,1), P(1,2), P(2,1), P(2,2));

fprintf('\n=== houghpeaks with Threshold=8 ===\n');
P2 = houghpeaks(H, 5, 'Threshold', 8);
fprintf('size(P2,1) = %d (expect 5)\n', size(P2,1));

fprintf('\n=== houghpeaks with NHoodSize=[3 3] ===\n');
P3 = houghpeaks(H, 5, 'NHoodSize', [3 3]);
fprintf('size(P3,1) = %d (expect 5)\n', size(P3,1));

fprintf('\n=== houghpeaks with antisymmetric Theta wrap ===\n');
P4 = houghpeaks(H, 2, 'Theta', T);
fprintf('size(P4) = [%d %d] (expect [2 2])\n', size(P4,1), size(P4,2));

clear
import compat.*

% bwpropfilt — filter components by region attribute.
BW = false(10, 10);
BW(2:4, 2:4) = true;
BW(6:8, 2:7) = true;
BW(2:3, 7:9) = true;

fprintf('=== Area range [7 15] ===\n');
fprintf('sum=%d (expect 9)\n', sum(sum(bwpropfilt(BW, 'Area', [7 15]))));

fprintf('=== Area top-1 ===\n');
fprintf('sum=%d (expect 18)\n', sum(sum(bwpropfilt(BW, 'Area', 1))));

fprintf('=== Area top-2 smallest ===\n');
fprintf('sum=%d (expect 15)\n', sum(sum(bwpropfilt(BW, 'Area', 2, 'smallest'))));

fprintf('=== Eccentricity [0.7 1.0] ===\n');
fprintf('sum=%d (expect 24)\n', sum(sum(bwpropfilt(BW, 'Eccentricity', [0.7 1.0]))));

fprintf('=== EulerNumber [0 1] ===\n');
fprintf('sum=%d (expect 33)\n', sum(sum(bwpropfilt(BW, 'EulerNumber', [0 1]))));

fprintf('=== Extent [0.95 1.0] ===\n');
fprintf('sum=%d (expect 33)\n', sum(sum(bwpropfilt(BW, 'Extent', [0.95 1.0]))));

fprintf('=== EquivDiameter [3 4] ===\n');
fprintf('sum=%d (expect 9)\n', sum(sum(bwpropfilt(BW, 'EquivDiameter', [3 4]))));

fprintf('=== ConvexArea [7 20] ===\n');
fprintf('sum=%d (expect 27)\n', sum(sum(bwpropfilt(BW, 'ConvexArea', [7 20]))));

fprintf('=== Solidity [0.95 1.0] ===\n');
fprintf('sum=%d (expect 33)\n', sum(sum(bwpropfilt(BW, 'Solidity', [0.95 1.0]))));

fprintf('=== MeanIntensity marker ===\n');
I = zeros(10, 10);
I(2:4, 2:4) = 100;
I(6:8, 2:7) = 200;
I(2:3, 7:9) = 50;
fprintf('sum=%d (expect 18)\n', sum(sum(bwpropfilt(BW, I, 'MeanIntensity', [150 255]))));

fprintf('=== CC struct input ===\n');
CC = bwconncomp(BW, 8);
CC2 = bwpropfilt(CC, 'Area', 1);
fprintf('NumObjects=%d (expect 1)\n', CC2.NumObjects);

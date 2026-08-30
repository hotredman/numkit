clear

% intlut — table lookup for integer images.

% --- uint8 → uint8: invert via LUT ---
fprintf('--- intlut(uint8, 256-LUT) ---\n');
LUT8  = uint8(255 - (0:255));
A8    = uint8([10 20 30; 40 50 60]);
B8    = intlut(A8, LUT8);
disp(double(B8));
fprintf('  expect: [245 235 225; 215 205 195]\n\n');

% --- uint8 → uint16: type promotion via LUT ---
fprintf('--- intlut(uint8, uint16-LUT) ---\n');
LUT16 = uint16((0:255) * 257);          % bit-replication 0xAB → 0xABAB
B16   = intlut(A8, LUT16);
fprintf('class(B): %s\n', class(B16));
disp(double(B16));
fprintf('  expect: uint16, [2570 5140 7710; 10280 12850 15420]\n\n');

% --- int16: shift-by-32768 indexing ---
fprintf('--- intlut(int16) ---\n');
LUT16i = int16((0:65535) - 32768);      % identity
A16i   = int16([-100 0 100]);
Bi     = intlut(A16i, LUT16i);
disp(double(Bi));
fprintf('  expect: [-100 0 100] (identity LUT)\n\n');

% --- 3-D: per-channel mapping ---
fprintf('--- intlut(uint8 RGB) ---\n');
RGB    = uint8(cat(3, [0 128; 255 64], [10 20; 30 40], [50 60; 70 80]));
LUTinv = uint8(255 - (0:255));
RGBinv = intlut(RGB, LUTinv);
fprintf('size: %s\n', mat2str(size(RGBinv)));
disp(double(RGBinv(:,:,1)));
fprintf('  expect: [255 127; 0 191] in plane 1\n');

clear

import compat.*

% --- Build a known PNG, then probe via imfinfo ---
A = uint8(zeros(7, 11, 3));
imwrite(A, 'tests/fixtures/_finfo_rgb.png');
info = imfinfo('tests/fixtures/_finfo_rgb.png');

fprintf('--- imfinfo on 7x11x3 PNG ---\n');
fprintf('  Filename = %s\n', info.Filename);
fprintf('  Format = %s (expect png)\n', info.Format);
fprintf('  Width = %d (expect 11)\n', info.Width);
fprintf('  Height = %d (expect 7)\n', info.Height);
fprintf('  NumberOfChannels = %d (expect 3)\n', info.NumberOfChannels);
fprintf('  ColorType = %s (expect truecolor)\n', info.ColorType);
fprintf('  FileSize = %d bytes (expect > 0)\n\n', info.FileSize);

% --- Grayscale BMP ---
G = uint8([0 50 100 150 200 250]);
G = repmat(G, 4, 1);  % 4x6 grayscale
imwrite(G, 'tests/fixtures/_finfo_gray.bmp');
info = imfinfo('tests/fixtures/_finfo_gray.bmp');
fprintf('--- imfinfo on 4x6 BMP grayscale (stb writes 24-bit BMP always) ---\n');
fprintf('  Format = %s (expect bmp)\n', info.Format);
fprintf('  Width = %d, Height = %d (expect 6 x 4)\n', ...
    info.Width, info.Height);
fprintf('  NumberOfChannels = %d (BMP-on-disk is always 3-channel)\n', info.NumberOfChannels);
fprintf('  ColorType = %s\n\n', info.ColorType);

% --- grayscale PNG (which DOES preserve 1 channel) ---
imwrite(G, 'tests/fixtures/_finfo_gray.png');
info = imfinfo('tests/fixtures/_finfo_gray.png');
fprintf('--- imfinfo on grayscale PNG ---\n');
fprintf('  NumberOfChannels = %d (expect 1)\n', info.NumberOfChannels);
fprintf('  ColorType = %s (expect grayscale)\n\n', info.ColorType);

% --- RGBA PNG ---
RGBA = uint8(zeros(2, 3, 4));
imwrite(RGBA, 'tests/fixtures/_finfo_rgba.png');
info = imfinfo('tests/fixtures/_finfo_rgba.png');
fprintf('--- imfinfo on 2x3x4 RGBA PNG ---\n');
fprintf('  NumberOfChannels = %d (expect 4)\n', info.NumberOfChannels);
fprintf('  ColorType = %s (expect truecoloralpha)\n\n', info.ColorType);

% --- Missing file ---
try
    imfinfo('tests/fixtures/_does_not_exist.png');
    fprintf('--- BUG: missing file did not error\n');
catch err
    fprintf('--- imfinfo missing file correctly errored\n');
end

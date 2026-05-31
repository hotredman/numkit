clear
import compat.*

% rgbwide2xyz / xyz2rgbwide — wide-gamut HDR colour smoke.
% Reference engine: MATLAB R2025b Image Processing Toolbox.

fprintf('== rgbwide2xyz ==\n');

xyz = rgbwide2xyz(uint16([64 64 64]), 10);
fprintf('  black10: [%.6g %.6g %.6g] (expect 0 0 0)\n', xyz(1), xyz(2), xyz(3));

xyz = rgbwide2xyz(uint16([940 940 940]), 10);
fprintf('  white10: [%.6g %.6g %.6g] (expect 0.95047 1 1.08883)\n', xyz(1), xyz(2), xyz(3));

xyz = rgbwide2xyz(uint16([940 64 64]), 10);
fprintf('  red10:   [%.6g %.6g %.6g] (expect 0.6370 0.2627 0)\n', xyz(1), xyz(2), xyz(3));

xyz = rgbwide2xyz(uint16([502 502 502]), 10);
fprintf('  gray10:  [%.6g %.6g %.6g] (expect 0.2467 0.2596 0.2826)\n', xyz(1), xyz(2), xyz(3));

xyz = rgbwide2xyz(uint16([3760 3760 3760]), 12);
fprintf('  white12: [%.6g %.6g %.6g] (expect 0.95047 1 1.08883)\n', xyz(1), xyz(2), xyz(3));

xyz = rgbwide2xyz(uint16([940 940 940]), 10, 'ColorSpace', 'BT.2100');
fprintf('  PQ_w10:  [%.6g %.6g %.6g] (expect 0.95047 1 1.08883)\n', xyz(1), xyz(2), xyz(3));

xyz = rgbwide2xyz(uint16([940 940 940]), 10, 'ColorSpace', 'BT.2100', 'LinearizationFcn', 'HLG');
fprintf('  HLG_w10: [%.6g %.6g %.6g] (expect ~0.95 1 ~1.089)\n', xyz(1), xyz(2), xyz(3));

fprintf('\n== xyz2rgbwide ==\n');
rgb = xyz2rgbwide([0.95047 1 1.08883], 10);
fprintf('  white -> [%d %d %d] (expect 940 940 940)\n', rgb(1), rgb(2), rgb(3));
rgb = xyz2rgbwide([0 0 0], 10);
fprintf('  black -> [%d %d %d] (expect 64 64 64)\n', rgb(1), rgb(2), rgb(3));
rgb = xyz2rgbwide([0.95047 1 1.08883], 12);
fprintf('  white12 -> [%d %d %d] (expect 3760 3760 3760)\n', rgb(1), rgb(2), rgb(3));

fprintf('\n== round trip ==\n');
orig = uint16([502 600 700]);
xyz  = rgbwide2xyz(orig, 10);
back = xyz2rgbwide(xyz, 10);
fprintf('  10-bit [502 600 700] -> [%d %d %d]\n', back(1), back(2), back(3));

orig = uint16([1500 2000 3000]);
xyz  = rgbwide2xyz(orig, 12);
back = xyz2rgbwide(xyz, 12);
fprintf('  12-bit [1500 2000 3000] -> [%d %d %d]\n', back(1), back(2), back(3));

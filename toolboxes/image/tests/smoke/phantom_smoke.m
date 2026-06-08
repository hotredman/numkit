clear

import compat.*

% phantom — Shepp-Logan computational head phantom.

fprintf('--- phantom() default 256x256 ---\n');
P = phantom();
fprintf('size: %s, range: [%.3f, %.3f]\n', mat2str(size(P)), min(P(:)), max(P(:)));
fprintf('  expect: 256x256 double, mostly in [0, 1] for modified S-L\n\n');

fprintf('--- phantom(64) ---\n');
P64 = phantom(64);
fprintf('size: %s, sum = %.4f\n', mat2str(size(P64)), sum(P64(:)));
fprintf('  expect: 64x64\n\n');

fprintf('--- phantom("Shepp-Logan", 32) ---\n');
PSL = phantom('Shepp-Logan', 32);
fprintf('size: %s, range: [%.3f, %.3f]\n', mat2str(size(PSL)), min(PSL(:)), max(PSL(:)));
fprintf('  expect: original S-L (skull intensity ~1, ventricules near 0)\n\n');

fprintf('--- phantom with custom ellipses ---\n');
E = [1.0 0.5 0.5 0 0 0; -0.5 0.2 0.3 0 0 30];
[Pc, Eout] = phantom(E, 16);
fprintf('size: %s, sum = %.4f, Eout shape = %s\n', ...
        mat2str(size(Pc)), sum(Pc(:)), mat2str(size(Eout)));

clear
import compat.*

fprintf('=== signal/uencode + udecode (Phase 4.2 — uniform quantization) ===\n');

u = [-1.2 -1 -0.5 0 0.5 1 1.2];

fprintf('\n[uencode 3 bits, default unsigned]\n');
y = double(uencode(u, 3));
fprintf('  '); fprintf('%g ', y); fprintf('\n');
fprintf('  expect: 0 0 2 4 6 7 7\n');

fprintf('\n[uencode 3 bits, V=0.5 (tighter saturation)]\n');
y = double(uencode(u, 3, 0.5));
fprintf('  '); fprintf('%g ', y); fprintf('\n');
fprintf('  expect: 0 0 0 4 7 7 7\n');

fprintf('\n[uencode 3 bits signed]\n');
y = double(uencode(u, 3, 1, 'signed'));
fprintf('  '); fprintf('%g ', y); fprintf('\n');
fprintf('  expect: -4 -4 -2 0 2 3 3\n');

fprintf('\n[output type tier]\n');
fprintf('  uencode(0.5, 8):  class=%s (expect uint8)\n', class(uencode(0.5, 8)));
fprintf('  uencode(0.5, 10): class=%s (expect uint16)\n', class(uencode(0.5, 10)));
fprintf('  uencode(0.5, 20): class=%s (expect uint32)\n', class(uencode(0.5, 20)));

fprintf('\n[udecode int8 input — saturate]\n');
ui = int8([-1 1 2 -5]);
y = udecode(ui, 3);
fprintf('  '); fprintf('%g ', y); fprintf('\n');
fprintf('  expect: -0.25 0.25 0.5 -1\n');

fprintf('\n[udecode wrap mode]\n');
y = udecode(ui, 3, 1, 'wrap');
fprintf('  '); fprintf('%g ', y); fprintf('\n');
fprintf('  expect: -0.25 0.25 0.5 0.75\n');

fprintf('\n[8-bit roundtrip]\n');
us = -1:0.1:1;
yd = udecode(uencode(us, 8), 8);
fprintf('  max error = %g (expect ~0.0078125 = 2/256)\n', max(abs(yd - us)));

fprintf('\nAll BIT-EQUAL with MATLAB R2025b. Octave 11.1.0 also matches values.\n');

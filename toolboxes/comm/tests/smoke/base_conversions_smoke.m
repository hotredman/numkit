clear
import compat.*

fprintf('=== base conversions ===\n');

fprintf('\n[bit2int]\n');
b = [1 0 1 0 1 1 0 0]';
fprintf('  bit2int(b, 4) (msbfirst default): '); disp(bit2int(b, 4)');
fprintf('  expect: [10 12]\n');
fprintf('  bit2int(b, 4, false) (lsbfirst): '); disp(bit2int(b, 4, false)');
fprintf('  expect: [5 3]\n');

fprintf('\n[int2bit]\n');
b2 = int2bit([10 12], 4);
fprintf('  int2bit([10 12], 4) =\n'); disp(b2);
fprintf('  expect (col-form): col1=[1 0 1 0]'', col2=[1 1 0 0]''\n');

fprintf('\n[bi2de — legacy LSB-first default]\n');
d1 = bi2de([1 0 1 0; 0 0 1 1]);
fprintf('  bi2de([1 0 1 0; 0 0 1 1]) = '); disp(d1');
fprintf('  expect: [5 12]\n');
d2 = bi2de([1 0 1 0; 0 0 1 1], 'left-msb');
fprintf('  bi2de left-msb: '); disp(d2');
fprintf('  expect: [10 3]\n');
d3 = bi2de([4 3; 1 2], 10);
fprintf('  bi2de base 10: '); disp(d3');
fprintf('  expect: [34 21]\n');

fprintf('\n[de2bi]\n');
b1 = de2bi([5 12]);
fprintf('  de2bi([5 12]) (auto-width LSB) =\n'); disp(b1);
b2 = de2bi([5 12], 5);
fprintf('  de2bi([5 12], 5) =\n'); disp(b2);
b3 = de2bi([5 12], 5, 'left-msb');
fprintf('  de2bi([5 12], 5, left-msb) =\n'); disp(b3);
% Empty width [] + custom base (used to throw on the [] arg).
fprintf('  de2bi(10, [], 3) = '); disp(de2bi(10, [], 3));
fprintf('  expect: [1 0 1]  (base-3, auto width)\n');
fprintf('  de2bi(10, 4, 3)  = '); disp(de2bi(10, 4, 3));
fprintf('  expect: [1 0 1 0]  (base-3, 4 digits)\n');

fprintf('\n[vec2mat]\n');
[m, p] = vec2mat([1 2 3 4 5 6 7 8 9 10], 4);
fprintf('  vec2mat([1..10], 4):\n'); disp(m);
fprintf('  padded = %d (expect 2)\n', p);
m2 = vec2mat([1 2 3 4 5], 3, 99);
fprintf('  vec2mat([1..5], 3, 99):\n'); disp(m2);

fprintf('\n[round-trip]\n');
orig = [3 7 1 5 2 6 0 4]';
packed = bit2int(int2bit(orig, 4), 4);
fprintf('  round-trip [3 7 1 5 2 6 0 4] via int2bit/bit2int(n=4): '); disp(packed');

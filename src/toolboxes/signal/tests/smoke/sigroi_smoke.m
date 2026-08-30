clear

fprintf('=== signal ROI utils ===\n');

m = logical([0 1 1 0 0 1 1 1 0 1]);
roi = binmask2sigroi(m);
fprintf('\nbinmask2sigroi([0110011101]):\n'); disp(roi);
fprintf('  expect [2 3; 6 8; 10 10]\n');

m2 = sigroi2binmask([2 3; 6 8; 10 10]);
fprintf('sigroi2binmask: '); fprintf('%d ', m2); fprintf(' (expect 0 1 1 0 0 1 1 1 0 1)\n');
m3 = sigroi2binmask([2 3; 6 8; 10 10], 12);
fprintf('  with len=12: '); fprintf('%d ', m3); fprintf(' (numel=%d)\n', numel(m3));

ext = extendsigroi([3 5; 8 10], 2, 1);
fprintf('\nextendsigroi([3 5; 8 10], 2, 1):\n'); disp(ext);
fprintf('  expect [1 6; 6 11]\n');

sh = shortensigroi([1 10; 12 20], 2, 3);
fprintf('shortensigroi([1 10; 12 20], 2, 3):\n'); disp(sh);

mer = mergesigroi([1 3; 4 6; 8 10; 9 12], 1);
fprintf('mergesigroi sep=1:\n'); disp(mer);
fprintf('  expect [1 12]\n');
mer0 = mergesigroi([1 3; 4 6; 8 10; 9 12], 0);
fprintf('mergesigroi sep=0:\n'); disp(mer0);
fprintf('  expect [1 6; 8 12]\n');

% removesigroi(roi, maxLen) — drops ROIs of length <= maxLen
rem = removesigroi([1 5; 7 7; 10 12], 2);
fprintf('removesigroi maxLen=2: drops [7 7] (len=1) → expect [1 5; 10 12]:\n'); disp(rem);
rem0 = removesigroi([1 3; 5 7; 10 12], 0);
fprintf('  maxLen=0 keeps all (no len<=0):\n'); disp(rem0);

x = (1:20)';
seg = extractsigroi(x, [3 5; 8 10], true);
fprintf('extractsigroi concat: '); fprintf('%g ', seg); fprintf(' (expect 3 4 5 8 9 10)\n');

% sigrangebinmask(x, bound)
%   bound scalar       → x > bound (default Relationship='above')
%   bound 2-vec [a b]  → a <= x <= b (default Relationship='inside')
xx = [1 2 3 4 5 4 3 2 1];
mr = sigrangebinmask(xx, 2);
fprintf('sigrangebinmask scalar bound=2 ("above"): '); fprintf('%d ', mr); fprintf('\n');
mri = sigrangebinmask(xx, [2 4]);
fprintf('sigrangebinmask 2-vec [2 4] ("inside"): '); fprintf('%d ', mri); fprintf('\n');

clear

% bwpack / bwunpack — pack 32 binary rows into uint32 columns.

% --- bwpack(eye(5)) → uint32([1 2 4 8 16]) ---
fprintf('--- bwpack(eye(5)) ---\n');
P = bwpack(eye(5));
fprintf('class = %s, size = %s\n', class(P), mat2str(size(P)));
disp(double(P));
fprintf('  expect: [1 2 4 8 16]\n\n');

% --- pad to 32 rows when H = 5 ---
fprintf('--- bwpack of 5x5: 1 packed row needed ---\n');
fprintf('size: %s (expect [1 5])\n\n', mat2str(size(P)));

% --- round-trip eye(33) → 2 packed rows → unpacked back ---
fprintf('--- round-trip eye(33) ---\n');
A = eye(33);
P2 = bwpack(A);
fprintf('packed size: %s (expect [2 33])\n', mat2str(size(P2)));
B = bwunpack(P2, 33);
fprintf('round-trip match = %d (expect 1)\n', isequal(double(B), A));

% --- bwunpack with default M ---
fprintf('\n--- bwunpack default M ---\n');
P3 = uint32([7 7 7]);
U  = bwunpack(P3);
fprintf('unpack default size: %s (expect [32 3])\n', mat2str(size(U)));
fprintf('first 4 rows of col 1: ');
disp(double(U(1:4, 1)));
fprintf('  expect: [1 1 1 0]\n');

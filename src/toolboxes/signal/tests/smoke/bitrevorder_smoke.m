clear

fprintf('=== bitrevorder ===\n');

% 1-output form
y = bitrevorder([10 20 30 40 50 60 70 80]);
fprintf('  bitrevorder(X) = '); fprintf('%g ', y); fprintf('\n');
fprintf('  expect:         10 50 30 70 20 60 40 80\n');

% 2-output form (bug fix 2026-05-08: I was missing)
[Y, I] = bitrevorder([1:8]);
fprintf('  [Y, I] = bitrevorder(1:8):\n');
fprintf('    Y = '); fprintf('%g ', Y); fprintf('\n');
fprintf('    I = '); fprintf('%g ', I); fprintf('\n');
fprintf('    expect both: 1 5 3 7 2 6 4 8\n');

% Algebraic identity Y == X(I)
X = [10 20 30 40 50 60 70 80];
[Yx, Ix] = bitrevorder(X);
fprintf('  identity Y - X(I): '); fprintf('%g ', Yx - X(Ix)); fprintf('\n');

% N=4
[Y4, I4] = bitrevorder([100 200 300 400]);
fprintf('  N=4: Y = '); fprintf('%g ', Y4); fprintf(', I = '); fprintf('%g ', I4); fprintf('\n');
fprintf('       expect: 100 300 200 400 / 1 3 2 4\n');

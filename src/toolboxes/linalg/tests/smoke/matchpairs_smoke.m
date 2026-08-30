clear

fprintf('=== matchpairs — Hungarian linear assignment ===\n');

% Square 3x3, high penalty so all 3 rows get matched.
fprintf('\nSquare 3x3:\n');
Cost = [4 1 3; 2 0 5; 3 2 2];
disp(Cost);
[M, uR, uC] = matchpairs(Cost, 100);
fprintf('M ='); disp(M);
total = 0;
for k = 1:size(M, 1)
    total = total + Cost(M(k, 1), M(k, 2));
end
fprintf('  total cost = %g  (optimal = 1+2+2 = 5)\n', total);

% Rectangular: 4 rows, 2 cols. At most 2 matches.
fprintf('\nRectangular 4x2:\n');
Cost = [10 20; 30 5; 25 8; 1 50];
[M2, uR2, uC2] = matchpairs(Cost, 100);
fprintf('M ='); disp(M2);
fprintf('unmatched rows ='); disp(uR2');
fprintf('  optimal pairs: (2,2)+(4,1), cost = 5+1 = 6\n');

% Low penalty — everything unmatched.
fprintf('\nLow penalty (0.5) → all unmatched:\n');
[Me, uRe, uCe] = matchpairs([10 20; 30 40], 0.5);
fprintf('M ='); disp(Me);
fprintf('uR ='); disp(uRe');
fprintf('uC ='); disp(uCe');

% Max mode. NB: in MATLAB 'max', costUnmatched is a REWARD for leaving
% unmatched (not a penalty), so a HIGH positive value LEAVES things
% unmatched. Use 0 / negative to force the full max assignment.
fprintf('\nmax mode with penalty 0 (forces all-matched):\n');
Cm = [1 5; 4 2];
[Mmax, ~, ~] = matchpairs(Cm, 0, 'max');
fprintf('M ='); disp(Mmax);
total = 0;
for k = 1:size(Mmax, 1)
    total = total + Cm(Mmax(k, 1), Mmax(k, 2));
end
fprintf('  total benefit = %g  (max = 5 + 4 = 9)\n', total);

fprintf('\nmax mode with HIGH reward (100) → all unmatched:\n');
[Mhi, uRhi, uChi] = matchpairs(Cm, 100, 'max');
fprintf('M='); disp(Mhi);
fprintf('uR='); disp(uRhi');
fprintf('uC='); disp(uChi');

% MATLAB doc example.
fprintf('\nMATLAB doc example — 3 detections / 3 targets, penalty 20:\n');
Cd = [10 15 9; 9 18 5; 6 14 3];
[Md, ~, ~] = matchpairs(Cd, 20);
fprintf('M ='); disp(Md);
total = 0;
for k = 1:size(Md, 1)
    total = total + Cd(Md(k, 1), Md(k, 2));
end
fprintf('  total cost = %g  (MATLAB: 26)\n', total);

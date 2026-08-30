clear

fprintf('=== lloyds (Lloyd-Max scalar quantizer designer) ===\n');

% Initial-codebook form
training = [1 2 3 4 5 6 7 8 9 10];
[partition, codebook, distor] = lloyds(training, [2 5 8]);
fprintf('  Initial codebook [2 5 8] on uniform 1..10:\n');
fprintf('  partition: ');  fprintf('%g ', partition);  fprintf('\n');
fprintf('  expected:  3.5 6.75\n');
fprintf('  codebook : ');  fprintf('%g ', codebook);  fprintf('\n');
fprintf('  expected : 2 5 8.5\n');
fprintf('  distor: %g (expect 0.9)\n', distor);

% Integer-K form
rng(42);
training2 = randn(1, 1000);
[p2, c2, d2, r2] = lloyds(training2, 4);
fprintf('\n  K=4 designer on N(0,1) sample (N=1000, seed=42):\n');
fprintf('  partition: ');  fprintf('%.4f ', p2);  fprintf('\n');
fprintf('  expected:  -0.9698 0.0679 1.1414\n');
fprintf('  codebook : ');  fprintf('%.4f ', c2);  fprintf('\n');
fprintf('  expected : -1.5180 -0.4217 0.5575 1.7253\n');
fprintf('  distor: %.6f (expect 0.137030)\n', d2);
fprintf('  rel: %.6f\n', r2);

% Round-trip via quantiz
qv = quantiz(training, partition, codebook);
[~, q2, dist3] = quantiz(training, partition, codebook);
fprintf('\n  quantiz round-trip with lloyds output: distor=%g (expect 0.9)\n', dist3);

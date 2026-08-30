clear

% lin2rgb — linear → sRGB gamma forward.

fprintf('--- 0.5 (above d=0.0031308 → power branch) ---\n');
v = lin2rgb(0.5);
fprintf('lin2rgb(0.5) = %.10f\n', v);
fprintf('  expect 1.055*0.5^(1/2.4)-0.055 = %.10f\n', 1.055*0.5^(1/2.4)-0.055);

fprintf('\n--- 0.001 (below threshold → linear branch) ---\n');
v = lin2rgb(0.001);
fprintf('lin2rgb(0.001) = %.10f\n', v);
fprintf('  expect 12.92*0.001 = %.10f\n', 12.92*0.001);

fprintf('\n--- 0 and 1 fixed points ---\n');
fprintf('lin2rgb(0) = %.10f  (expect 0)\n', lin2rgb(0));
fprintf('lin2rgb(1) = %.10f  (expect 1)\n', lin2rgb(1));

fprintf('\n--- round-trip rgb2lin/lin2rgb ---\n');
A = [0 0.1 0.3 0.5 0.7 0.95 1];
B = lin2rgb(rgb2lin(A));
fprintf('A:        %s\n', mat2str(A, 6));
fprintf('round-A:  %s\n', mat2str(B, 6));
fprintf('max|A-B| = %.3e\n', max(abs(A-B)));

fprintf('\n--- negative input → mirrored ---\n');
fprintf('lin2rgb(-0.5) = %.10f  (expect -0.7353569831)\n', lin2rgb(-0.5));

clear

% ifft2(X, 'symmetric') (DEEP-PROBE 2026-05-31). Previously numkit threw
% "Cannot convert char to scalar" (the flag was parsed as a size arg). MATLAB
% treats X as conjugate-symmetric so the 2-D inverse is EXACTLY REAL. This
% decomposes into the 1-D ifft 'symmetric' over each dim of length > 1 — note
% it is NOT real(ifft2(X)), which differs for a genuine 2-D matrix. vs MATLAB
% R2025b. (The resize form ifft2(X,m,n,'symmetric') is a deferred gap.)

X1 = [1+1i 2-3i 5; 4 5+2i 1-1i];
fprintf('=== 2x3 matrix ===\n');
A = ifft2(X1, 'symmetric');
fprintf('isreal = %d (expect 1)\n', isreal(A));
disp('ifft2(X1,"symmetric") — expect [3.16667 -0.0446582 -0.622008; -1.5 1.44338 -1.44338]:');
disp(A);

fprintf('=== column vector (reduces to 1-D symmetric over dim 1) ===\n');
B = ifft2([1+2i; 3-1i; 5+0.5i; 2i], 'symmetric');
fprintf('B = [%g; %g; %g; %g]  (expect [3; -0.5; 0; -1.5])\n', B(1), B(2), B(3), B(4));

fprintf('=== scalar ===\n');
fprintf('ifft2(3+4i,"symmetric") = %g  (expect 3)\n', ifft2(3+4i, 'symmetric'));

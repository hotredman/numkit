clear

% norm() of a COMPLEX array — by element magnitude (fixed 2026-06-05).
% Previously threw "Not a double array".

v = [3+4i 0 -2i];                          % |.| = [5 0 2]
fprintf('norm(v)       = %.6g   (expect 5.38516 = sqrt(29))\n', norm(v));
fprintf('norm(v,1)     = %.6g   (expect 7)\n', norm(v, 1));
fprintf('norm(v,Inf)   = %.6g   (expect 5)\n', norm(v, Inf));
fprintf('norm(v,''fro'') = %.6g   (expect 5.38516)\n', norm(v, 'fro'));
fprintf('norm(3+4i)    = %.6g   (expect 5)\n', norm(3+4i));

M = [1+1i 2; 3 4-1i];
fprintf('norm(M,1)     = %.6g   (expect 6.12311 = 2+sqrt(17))\n', norm(M, 1));
fprintf('norm(M,Inf)   = %.6g   (expect 7.12311 = 3+sqrt(17))\n', norm(M, Inf));
fprintf('norm(M,''fro'') = %.6g   (expect 5.65685 = sqrt(32))\n', norm(M, 'fro'));

% Complex matrix 2-norm (spectral) needs a complex SVD -> still errors.
try
    norm(M, 2);
    fprintf('norm(M,2): UNEXPECTEDLY returned a value\n');
catch e
    fprintf('norm(M,2): errors as expected (complex SVD gap)\n');
end

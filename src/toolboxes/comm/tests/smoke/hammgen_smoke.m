clear

% hammgen — parity-check / generator matrices for a Hamming code.
% Error Correction Codes / block linear codes, GF(2).

% (7,4) Hamming code.
[h, g, n, k] = hammgen(3);
fprintf('hammgen(3): n=%d k=%d  (expect n=7 k=4)\n', n, k);
disp('H ='); disp(h);
% expect [1 0 0 1 0 1 1; 0 1 0 1 1 1 0; 0 0 1 0 1 1 1]
disp('G ='); disp(g);
% expect [1 1 0 1 0 0 0; 0 1 1 0 1 0 0; 1 1 1 0 0 1 0; 1 0 1 0 0 0 1]

% Systematic: first m columns of H are the identity.
fprintf('H(:,1:3) == eye(3):  %d  (expect 1)\n', isequal(h(:,1:3), eye(3)));
% Orthogonal generator/parity matrices.
fprintf('G*H'' == 0 (mod 2):   %d  (expect 1)\n', all(all(mod(g * h', 2) == 0)));

% (15,11) Hamming code dimensions.
[h4, g4, n4, k4] = hammgen(4);
fprintf('hammgen(4): n=%d k=%d  (expect n=15 k=11)\n', n4, k4);

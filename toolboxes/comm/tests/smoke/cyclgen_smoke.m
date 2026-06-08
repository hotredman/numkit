clear
import compat.*

% cyclgen — parity-check / generator matrices for a cyclic code.
% Error Correction Codes / block linear codes, GF(2).

p = [1 0 1 1];   % generator polynomial 1 + x^2 + x^3 (divides x^7 - 1)

% Systematic (default).
[h, g, k] = cyclgen(7, p);
fprintf('cyclgen(7,[1 0 1 1]): k=%d  (expect 4)\n', k);
disp('h ='); disp(h);
% expect [1 0 0 1 1 1 0; 0 1 0 0 1 1 1; 0 0 1 1 1 0 1]
disp('g ='); disp(g);
% expect [1 0 1 1 0 0 0; 1 1 1 0 1 0 0; 1 1 0 0 0 1 0; 0 1 1 0 0 0 1]
fprintf('g(:,4:7) == eye(4): %d  (expect 1)\n', isequal(g(:,4:7), eye(4)));
fprintf('g*h'' == 0 (mod 2):  %d  (expect 1)\n', all(all(mod(g*h', 2) == 0)));

% Non-systematic (cyclic shifts).
[hn, gn] = cyclgen(7, p, 'nonsystem');
disp('nonsystem g ='); disp(gn);
% expect [1 0 1 1 0 0 0; 0 1 0 1 1 0 0; 0 0 1 0 1 1 0; 0 0 0 1 0 1 1]
fprintf('nonsystem g*h''==0:  %d  (expect 1)\n', all(all(mod(gn*hn', 2) == 0)));

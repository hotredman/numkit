clear
import compat.*

% gen2par — convert a generator matrix to its parity-check matrix (and back).
% Error Correction Codes / block linear codes, GF(2).

% Hamming(7,4) generator [P | I_4].
G = [1 1 0 1 0 0 0; 0 1 1 0 1 0 0; 1 1 1 0 0 1 0; 1 0 1 0 0 0 1];
H = gen2par(G);
disp('H = gen2par(G):');
disp(H);
% expect [1 0 0 1 0 1 1; 0 1 0 1 1 1 0; 0 0 1 0 1 1 1]

% Orthogonality: G * H' == 0 (mod 2).
fprintf('G*H'' == 0 (mod 2): %d  (expect 1)\n', all(all(mod(G * H', 2) == 0)));

% Involution: gen2par(gen2par(G)) recovers G.
fprintf('gen2par involution:  %d  (expect 1)\n', isequal(gen2par(H), G));

% [I_3 | P] form -> [P' | I_2].
G2 = [1 0 0 1 1; 0 1 0 0 1; 0 0 1 1 0];
disp('gen2par([I_3|P]):');
disp(gen2par(G2));   % expect [1 0 1 1 0; 1 1 0 0 1]

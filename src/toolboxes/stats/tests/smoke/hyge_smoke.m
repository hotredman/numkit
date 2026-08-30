clear

% Hypergeometric: M = pop, K = successes in pop, N = sample size
% C(K, k) C(M-K, N-k) / C(M, N)
% hygepdf(2, 50, 20, 5) — draw 5 from {20 successes, 30 failures}, get 2:
% C(20,2) * C(30,3) / C(50,5) = 190 * 4060 / 2118760 = 771400/2118760 = 0.36419
fprintf('hygepdf(2, 50, 20, 5)  = %.6f  (expect 0.364182)\n', hygepdf(2, 50, 20, 5));

% hygecdf(2, 50, 20, 5):
% pmf(0) = C(30,5)/C(50,5) = 142506/2118760 = 0.067263
% pmf(1) = C(20,1)C(30,4)/C(50,5) = 20*27405/2118760 = 0.258704
% pmf(2) = 0.364182
% Sum = 0.690149
fprintf('hygecdf(2, 50, 20, 5)  = %.6f  (expect 0.690149)\n', hygecdf(2, 50, 20, 5));

% inv: round-trip
fprintf('hygeinv(hygecdf(3, 100, 30, 10), 100, 30, 10) = %.6f  (expect 3)\n', ...
    hygeinv(hygecdf(3, 100, 30, 10), 100, 30, 10));

% stat
% hygestat(50, 20, 5): mean = 5*20/50 = 2, var = 5*20*30*45 / (2500*49) = 1.1020
[m, v] = hygestat(50, 20, 5);
fprintf('hygestat(50, 20, 5)    = [%.4f, %.4f]  (expect [2, 1.1020])\n', m, v);

% rnd
N = 50000;
X = hygernd(50, 20, 5, N, 1);
fprintf('hygernd(50,20,5) N=%d: mean=%.4f var=%.4f (expect 2, 1.1020)\n', N, mean(X), var(X));

clear

Sigma = [2 0.3; 0.3 1];
df = 8;
rng(0);

W = wishrnd(Sigma, df);
fprintf('wishrnd single 2x2 draw at df=8:\n');
disp(W);
fprintf('Should be symmetric, positive-definite. det = %.4f (expect > 0)\n', det(W));

% Mean of many draws — should converge to df * Sigma.
acc = zeros(2);
N = 600;
for i = 1:N
    acc = acc + wishrnd(Sigma, df);
end
M = acc / (N * df);
fprintf('\nwishrnd mean(W)/df over %d draws (~ Sigma = [2,0.3; 0.3,1]):\n', N);
disp(M);

% iwishrnd: E[W] = Tau / (df - p - 1)
Tau = [2 0.3; 0.3 1];
df2 = 7;
acc = zeros(2);
for i = 1:N
    acc = acc + iwishrnd(Tau, df2);
end
M = acc / N * (df2 - 3);   % * (df - p - 1)
fprintf('\niwishrnd mean(W)*(df-p-1) over %d draws (~ Tau = [2,0.3; 0.3,1]):\n', N);
disp(M);

% Benchmark linsolve (Cholesky)
rng(0);
N512 = 512;
A512 = randn(N512, N512);
A512 = A512 * A512';
b512 = randn(N512, 1);
opt.SYM = true;
opt.POSDEF = true;

% Warmup
linsolve(A512, b512, opt);

tic;
for i = 1:50
    linsolve(A512, b512, opt);
end
t512 = toc / 50.0;
fprintf('N=512: %f ms\n', t512 * 1000);

N1024 = 1024;
A1024 = randn(N1024, N1024);
A1024 = A1024 * A1024';
b1024 = randn(N1024, 1);

% Warmup
linsolve(A1024, b1024, opt);

tic;
for i = 1:10
    linsolve(A1024, b1024, opt);
end
t1024 = toc / 10.0;
fprintf('N=1024: %f ms\n', t1024 * 1000);

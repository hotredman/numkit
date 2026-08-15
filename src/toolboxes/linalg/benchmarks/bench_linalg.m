% bench_linalg.m - MATLAB performance comparison script for NumKit Linalg
sizes = [64, 128, 256, 512, 1024, 2048];
fprintf('=== MATLAB Linear Algebra Benchmark ===\n');
fprintf('Date: %s\n', char(datetime('now')));
fprintf('MATLAB Version: %s\n', version);
fprintf('Max Threads: %d\n\n', maxNumCompThreads());

for n = sizes
    A = randn(n);
    B = randn(n);
    A_c = randn(n) + 1i * randn(n);
    B_c = randn(n) + 1i * randn(n);
    b = randn(n, 1);
    b_c = randn(n, 1) + 1i * randn(n, 1);
    A_pos = A' * A + n * eye(n);

    % GEMM Real
    f_gemm = @() A * B;
    t_gemm = timeit(f_gemm);
    gflops_gemm = (2 * n^3) / (t_gemm * 1e9);

    % LU Real
    f_lu = @() lu(A);
    t_lu = timeit(f_lu);

    % Chol Real
    f_chol = @() chol(A_pos);
    t_chol = timeit(f_chol);

    % Complex LU
    f_lu_c = @() lu(A_c);
    t_lu_c = timeit(f_lu_c);

    % Solve Real
    f_solve = @() A \ b;
    t_solve = timeit(f_solve);

    fprintf('N=%4d | GEMM: %6.2f ms (%6.2f GFLOPS) | LU: %6.2f ms | Chol: %6.2f ms | Complex LU: %6.2f ms | Solve: %6.2f ms\n', ...
            n, t_gemm * 1000, gflops_gemm, t_lu * 1000, t_chol * 1000, t_lu_c * 1000, t_solve * 1000);
end

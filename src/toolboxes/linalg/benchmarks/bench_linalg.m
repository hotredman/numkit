% bench_linalg.m - MATLAB performance comparison script for NumKit Linalg
sizes = [64, 128, 256, 512, 1024, 2048];
fprintf('=== MATLAB Linear Algebra Benchmark ===\n');
fprintf('Max Threads: %d\n\n', maxNumCompThreads());

for n = sizes
    A = randn(n);
    B = randn(n);
    A_c = randn(n) + 1i * randn(n);
    B_c = randn(n) + 1i * randn(n);
    b = randn(n, 1);
    b_c = randn(n, 1) + 1i * randn(n, 1);
    A_pos = A' * A + n * eye(n);
    
    % Warmup
    [L, U, P] = lu(A);
    
    % GEMM Real
    t_gemm = inf;
    for rep = 1:5
        tic; C = A * B; t = toc;
        t_gemm = min(t_gemm, t);
    end
    gflops_gemm = (2 * n^3) / (t_gemm * 1e9);
    
    % LU Real
    t_lu = inf;
    for rep = 1:5
        tic; [L, U, P] = lu(A); t = toc;
        t_lu = min(t_lu, t);
    end
    
    % Chol Real
    t_chol = inf;
    for rep = 1:5
        tic; R = chol(A_pos); t = toc;
        t_chol = min(t_chol, t);
    end
    
    % Complex LU
    t_lu_c = inf;
    for rep = 1:5
        tic; [L, U, P] = lu(A_c); t = toc;
        t_lu_c = min(t_lu_c, t);
    end
    
    % Solve Real
    t_solve = inf;
    for rep = 1:5
        tic; x = A \ b; t = toc;
        t_solve = min(t_solve, t);
    end
    
    fprintf('N=%4d | GEMM: %6.2f ms (%6.2f GFLOPS) | LU: %6.2f ms | Chol: %6.2f ms | Complex LU: %6.2f ms | Solve: %6.2f ms\n', ...
            n, t_gemm * 1000, gflops_gemm, t_lu * 1000, t_chol * 1000, t_lu_c * 1000, t_solve * 1000);
end

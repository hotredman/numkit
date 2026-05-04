import compat.*

% Round-trip: idct2(dct2(A)) == A
rng(42);
A = round(100 * rand(8, 8));
B = dct2(A);
A2 = idct2(B);
fprintf('--- dct2/idct2 round-trip on 8x8 random ---\n');
fprintf('max|A - idct2(dct2(A))| = %.6e (expect ~ 1e-12)\n\n', max(max(abs(A - A2))));

% DC content concentrated in B(1,1) (orthonormal DCT)
fprintf('--- B(1,1) = %.4f ---\n', B(1,1));
fprintf('  expect: sum(A(:)) / 8 = %.4f\n\n', sum(A(:))/8);

% Constant image: only B(1,1) is non-zero, equal to N*sqrt(1/N)*v = sqrt(N)*v
C = 5 * ones(8, 8);
D = dct2(C);
fprintf('--- dct2(5*ones(8,8))(1,1) = %.4f (expect %.4f = 5*8) ---\n', D(1,1), 5*8);
fprintf('     max of off-DC = %.6e (expect ~ 0)\n\n', ...
    max(max(abs(D - [D(1,1) zeros(1,7); zeros(7,1) zeros(7,7)]))));

% dctmtx(N): D'*D == eye(N) (orthonormal); D*x is 1-D dct(x)
N = 8;
D = dctmtx(N);
I = D'*D;
fprintf('--- dctmtx(8)''*dctmtx(8) ---\n');
fprintf('max|D''*D - I| = %.6e (expect ~ 1e-15)\n\n', max(max(abs(I - eye(N)))));

% D applied to a column should match signal::dct
x = [1 2 3 4 5 6 7 8]';
y_via_mat = D * x;
y_via_dct = dct(x);
fprintf('--- max|D*x - dct(x)| = %.6e (expect ~ 1e-13) ---\n', ...
    max(abs(y_via_mat - y_via_dct)));

% Non-square 2-D round-trip
R = randn(4, 7);
fprintf('--- non-square idct2(dct2) round-trip on randn(4,7): max err = %.6e ---\n', ...
    max(max(abs(R - idct2(dct2(R))))));

clear
% Smoke test for complex LU, det, inv, and mldivide

B = [1+1i, 2; 3, 4-1i];
b = [1; 2+1i];

d = det(B);
disp(d);

[L, U, P] = lu(B);
disp(L);
disp(U);
disp(P);

Binv = inv(B);
disp(Binv);

x = B \ b;
disp(x);

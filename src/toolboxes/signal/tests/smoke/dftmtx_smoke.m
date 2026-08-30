clear

fprintf('=== dftmtx ===\n');

F1 = dftmtx(1);
fprintf('  F1 = %g (expect 1)\n', F1);

F2 = dftmtx(2);
fprintf('  F2 =\n'); disp(F2);
fprintf('  expect: [1 1; 1 -1]\n');

F4 = dftmtx(4);
fprintf('  F4(1, :) = [%g %g %g %g] (expect all 1)\n', ...
    real(F4(1,1)), real(F4(1,2)), real(F4(1,3)), real(F4(1,4)));
fprintf('  F4(2, 2) = %g + %gi (expect 0 - 1i)\n', real(F4(2,2)), imag(F4(2,2)));
fprintf('  F4(2, 3) = %g + %gi (expect -1 + 0i)\n', real(F4(2,3)), imag(F4(2,3)));

F8 = dftmtx(8);
fprintf('  F8 size  = %d × %d\n', size(F8, 1), size(F8, 2));
fprintf('  F8(2, 2) = %.4f + %.4fi (expect 0.7071 - 0.7071i)\n', real(F8(2,2)), imag(F8(2,2)));

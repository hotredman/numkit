clear

fprintf('=== gausswin ===\n');
fprintf('  default α=2.5, N=8 : '); fprintf('%.4f ', gausswin(8)); fprintf('\n');
fprintf('  expect: 0.0439 0.2030 0.5633 0.9382 0.9382 0.5633 0.2030 0.0439\n');

fprintf('  α=1.5, N=8         : '); fprintf('%.4f ', gausswin(8, 1.5)); fprintf('\n');
fprintf('  α=4,   N=16 endpoint=%.6f, center=%.4f\n', ...
    gausswin(16, 4)(1), gausswin(16, 4)(8));
fprintf('  α=8,   N=64 endpoint=%g (very small), center=%.4f\n', ...
    gausswin(64, 8)(1), gausswin(64, 8)(32));

fprintf('  N=1 (single-pt)    : %g (expect 1)\n', gausswin(1, 4));

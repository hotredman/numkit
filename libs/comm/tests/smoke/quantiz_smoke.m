clear
import compat.*

fprintf('=== quantiz (scalar quantizer) ===\n');

sig = [-2.4 -0.7 0.1 0.5 1.5 3.0];
partition = [-1 -0.5 0 0.5 1];
codebook  = [-1.5 -0.75 -0.25 0.25 0.75 1.5];

[indx, quantv, distor] = quantiz(sig, partition, codebook);
fprintf('  indx:    '); fprintf('%d ', indx); fprintf('\n');
fprintf('  expected: 0 1 3 3 5 5\n');
fprintf('  quantv:   '); fprintf('%g ', quantv); fprintf('\n');
fprintf('  expected: -1.5 -0.75 0.25 0.25 1.5 1.5\n');
fprintf('  distor:   %g\n', distor);
fprintf('  expected: 0.524583\n');

% 2-arg form (just indx)
i2 = quantiz(sig, partition);
fprintf('  just indx: '); fprintf('%d ', i2); fprintf('\n');

% Orientation
ir = quantiz([1 2 3], [1.5 2.5]);
fprintf('  row sig -> indx shape [%d %d] (expect 1 3)\n', size(ir, 1), size(ir, 2));
ic = quantiz([1; 2; 3], [1.5 2.5]);
fprintf('  col sig -> indx shape [%d %d] (expect 3 1)\n', size(ic, 1), size(ic, 2));

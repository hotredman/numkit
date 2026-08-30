clear

fprintf('=== dpcmenco / dpcmdeco (DPCM codec round-trip) ===\n');

predictor = [0 1];
partition = [-1 -0.5 0 0.5 1];
codebook  = [-1.5 -0.75 -0.25 0.25 0.75 1.5];

sig = [0.1 0.5 0.7 0.6 0.2 -0.3 -0.5];

[indx, qe] = dpcmenco(sig, codebook, partition, predictor);
fprintf('  encode indx: ');
fprintf('%d ', indx);
fprintf('\n  expected   : 3 3 3 2 2 1 2\n');
fprintf('  quanterr   : ');
fprintf('%g ', qe);
fprintf('\n');

[sigout, qe2] = dpcmdeco(indx, codebook, predictor);
fprintf('  decode sig : ');
fprintf('%g ', sigout);
fprintf('\n  expected   : 0.25 0.5 0.75 0.5 0.25 -0.5 -0.75\n');

fprintf('  original   : ');
fprintf('%g ', sig);
fprintf('\n  MSE = %g (expected ~0.02)\n', mean((sig - sigout).^2));

% qe vs qe2 consistency
fprintf('  qe == qe2 : %d (expect 1)\n', isequal(qe, qe2));

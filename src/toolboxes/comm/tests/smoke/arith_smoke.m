clear

fprintf('=== arithenco / arithdeco (arithmetic codec round-trip) ===\n');

counts = [10 5 3 2];
seq = [1 2 1 3 4 1 1 2];

code = arithenco(seq, counts);
fprintf('  encoded bits (len=%d): ', length(code));
fprintf('%d ', code);
fprintf('\n  expected MATLAB: 0 1 0 0 1 1 1 0 0 0 1 0 1 0 0 0 0 0 0 0 0  (len=21)\n');

dseq = arithdeco(code, counts, length(seq));
fprintf('  decoded: ');
fprintf('%d ', dseq);
fprintf('\n  match: %d (expect 1)\n', isequal(seq, dseq));

% Larger round-trip
seq2 = [1 1 1 2 2 2 3 3 4 4 4 4 1 2 3 4 1 2 3 4 1 1 1 1 2];
code2 = arithenco(seq2, counts);
dseq2 = arithdeco(code2, counts, length(seq2));
fprintf('\n  longer round-trip: len(seq)=%d len(code)=%d match=%d\n', ...
        length(seq2), length(code2), isequal(seq2, dseq2));

% Row vs col
sigr = [1 2 3 4];
codr = arithenco(sigr, counts);
fprintf('  row sig -> code shape [%d %d]\n', size(codr, 1), size(codr, 2));
sigc = sigr';
codc = arithenco(sigc, counts);
fprintf('  col sig -> code shape [%d %d]\n', size(codc, 1), size(codc, 2));

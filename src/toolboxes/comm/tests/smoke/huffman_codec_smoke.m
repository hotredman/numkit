clear

fprintf('=== huffmanenco / huffmandeco (Huffman codec round-trip) ===\n');

% Build dict and round-trip a signal.
[dict, av] = huffmandict([1 2 3 4 5], [0.4 0.2 0.2 0.1 0.1]);
sig = [1 1 2 3 4 5 1];
enc = huffmanenco(sig, dict);
fprintf('  sig (length %d): ', length(sig));
fprintf('%d ', sig);
fprintf('\n  encoded (length %d): ', length(enc));
fprintf('%d ', enc);
fprintf('\n  avglen=%g, theoretical bits=%g\n', av, av * length(sig));

dec = huffmandeco(enc, dict);
fprintf('  decoded: ');
fprintf('%d ', dec);
fprintf('\n  match: %d\n', isequal(sig(:), dec(:)));

% Row vs column orientation
sigr = [1 2 3 4 5];
encr = huffmanenco(sigr, dict);
fprintf('\n  row sig -> enc shape [%d %d]\n', size(encr, 1), size(encr, 2));
sigc = sigr';
encc = huffmanenco(sigc, dict);
fprintf('  col sig -> enc shape [%d %d]\n', size(encc, 1), size(encc, 2));

% Round-trip preserves shape
decc = huffmandeco(encc, dict);
fprintf('  col enc -> dec shape [%d %d]  (expect [5 1])\n', size(decc, 1), size(decc, 2));

% 2-symbol minimal case
[d2, ~] = huffmandict([0 1], [0.7 0.3]);
sig2 = [0 1 1 0 1 0 0];
enc2 = huffmanenco(sig2, d2);
fprintf('\n  2-symbol sig %d bits, enc %d bits\n', length(sig2), length(enc2));
fprintf('  round-trip match: %d\n', isequal(sig2, huffmandeco(enc2, d2)));

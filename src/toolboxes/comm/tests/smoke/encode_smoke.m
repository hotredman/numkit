clear

% encode — linear block encoder (Error Correction Codes, GF(2)).

% Hamming(7,4), binary.
c = encode([1 0 1 1], 7, 4, 'hamming/binary');
fprintf('encode hamming [1 0 1 1]: '); disp(c);   % expect [1 0 0 1 0 1 1]

% Two message words at once.
c2 = encode([1 0 1 1 0 1 0 0], 7, 4, 'hamming/binary');
fprintf('encode 2 words:           '); disp(c2);
% expect [1 0 0 1 0 1 1 0 1 1 0 1 0 0]

% Cyclic (7,4) — generator polynomial defaults to cyclpoly(7,4).
fprintf('encode cyclic [1 0 1 1]:  '); disp(encode([1 0 1 1], 7, 4, 'cyclic/binary'));
% expect [0 0 0 1 0 1 1]

% Linear with explicit generator matrix.
[~, g] = hammgen(3);
fprintf('encode linear:            '); disp(encode([1 0 1 1], 7, 4, 'linear/binary', g));

% Decimal: one integer per word in and out.
fprintf('encode 11 hamming/decimal = %g  (expect 88)\n', ...
        encode(11, 7, 4, 'hamming/decimal'));

% Zero-padding report.
[~, added] = encode([1 0 1 1 0], 7, 4, 'hamming/binary');
fprintf('5-bit msg, added zeros    = %g  (expect 3)\n', added);

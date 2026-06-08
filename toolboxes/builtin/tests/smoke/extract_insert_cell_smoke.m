clear

import compat.*

% extractBefore / extractAfter / insertAfter / insertBefore on a CELL str —
% DEEP-PROBE 2026-05-31. All four rejected a cell first argument as "Not a
% char array". MATLAB processes a cell str element-wise (cell of char vectors,
% same shape); the scalar position/substring anchor and inserted text
% broadcast to every element. Reference: MATLAB R2025b.

fprintf('=== extractBefore / extractAfter (numeric pos) ===\n');
eb = extractBefore({'hello','world'}, 3);
ea = extractAfter({'hello','world'}, 3);
fprintf('extractBefore 3 -> {%s, %s}  (expect he, wo)\n', eb{1}, eb{2});
fprintf('extractAfter  3 -> {%s, %s}  (expect lo, ld)\n', ea{1}, ea{2});

fprintf('\n=== extract (substring anchor) ===\n');
ebs = extractBefore({'a-b','c-d'}, '-');
eas = extractAfter({'a-b','c-d'}, '-');
fprintf('before "-" -> {%s, %s}  (expect a, c)\n', ebs{1}, ebs{2});
fprintf('after  "-" -> {%s, %s}  (expect b, d)\n', eas{1}, eas{2});

fprintf('\n=== insertAfter / insertBefore ===\n');
ia = insertAfter({'ab','cd'}, 1, 'X');
ib = insertBefore({'ab','cd'}, 2, 'X');
ias = insertAfter({'a.b','c.d'}, '.', 'X');
fprintf('insertAfter 1 X -> {%s, %s}  (expect aXb, cXd)\n', ia{1}, ia{2});
fprintf('insertBefore 2 X -> {%s, %s}  (expect aXb, cXd)\n', ib{1}, ib{2});
fprintf('insertAfter "." X -> {%s, %s}  (expect a.Xb, c.Xd)\n', ias{1}, ias{2});

fprintf('\n=== shape preserved + scalar paths unchanged ===\n');
cc = extractBefore({'ab';'cd'}, 2);
fprintf('column cell -> size %dx%d  (expect 2x1)\n', size(cc,1), size(cc,2));
fprintf('scalar extractBefore(hello,3) = %s (class %s)\n', extractBefore('hello',3), class(extractBefore('hello',3)));

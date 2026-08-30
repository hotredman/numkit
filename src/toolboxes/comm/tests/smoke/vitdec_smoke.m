clear

% vitdec — hard-decision Viterbi decoder (Error Correction Codes).
% Round-trips with convenc and corrects isolated bit errors.

t = poly2trellis(3, [6 7]);            % rate 1/2, K=3
msg = [1 1 0 1 1 0 0 1 0 1 1 0];

code = convenc(msg, t);
dec  = vitdec(code, t, 12, 'trunc', 'hard');
fprintf('clean round-trip exact: %d  (expect 1)\n', isequal(dec, msg));

% Flip one coded bit -> Viterbi still recovers the message.
code(3) = 1 - code(3);
dec2 = vitdec(code, t, 12, 'trunc', 'hard');
fprintf('1 bit-error corrected:  %d  (expect 1)\n', isequal(dec2, msg));

% Standard K=7 code.
t7 = poly2trellis(7, [171 133]);
m7  = [1 0 1 1 0 0 1 0 1 1 1 0 0 1];
fprintf('K=7 round-trip exact:   %d  (expect 1)\n', ...
        isequal(vitdec(convenc(m7, t7), t7, 30, 'trunc', 'hard'), m7));

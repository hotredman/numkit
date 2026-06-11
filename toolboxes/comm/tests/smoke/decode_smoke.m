clear
import compat.*

% decode — linear block syndrome decoder (Error Correction Codes, GF(2)).

% Hamming(7,4) round-trip, clean channel.
cw = encode([1 0 1 1], 7, 4, 'hamming/binary');
[m, e] = decode(cw, 7, 4, 'hamming/binary');
fprintf('clean: msg='); disp(m); fprintf('   errors=%g  (expect 0)\n', e);

% Flip one coded bit -> still recovered, err reports 1.
rc = cw; rc(2) = 1 - rc(2);
[m2, e2] = decode(rc, 7, 4, 'hamming/binary');
fprintf('1 err: msg='); disp(m2); fprintf('   errors=%g  (expect 1)\n', e2);

% Every single-bit error position is correctable.
ok = 1;
for p = 1:7
    rcp = cw; rcp(p) = 1 - rcp(p);
    ok = ok && isequal(decode(rcp, 7, 4, 'hamming/binary'), [1 0 1 1]);
end
fprintf('all single errors fixed: %d  (expect 1)\n', ok);

% Decimal round-trip.
fprintf('decode 88 hamming/decimal = %g  (expect 11)\n', ...
        decode(88, 7, 4, 'hamming/decimal'));

% Cyclic round-trip with a corrected error.
ccw = encode([1 0 1 1], 7, 4, 'cyclic/binary'); ccw(4) = 1 - ccw(4);
fprintf('cyclic 1-err msg='); disp(decode(ccw, 7, 4, 'cyclic/binary'));

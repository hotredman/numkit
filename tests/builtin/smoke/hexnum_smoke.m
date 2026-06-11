clear
import compat.*

% hex2num / num2hex — IEEE-754 hexadecimal <-> floating-point bit pattern.
% NOTE: this is the raw BIT PATTERN, not the decimal value of the digits
% (that is hex2dec / dec2hex).

% ── hex2num: hex bits -> double ─────────────────────────────────
fprintf('hex2num(''3ff0000000000000'') = %g  (expect 1)\n', hex2num('3ff0000000000000'));
fprintf('hex2num(''bff0000000000000'') = %g  (expect -1)\n', hex2num('bff0000000000000'));
fprintf('hex2num(''4'')  = %g  (expect 2,  right-padded to 4000...)\n', hex2num('4'));
fprintf('hex2num(''41'') = %g  (expect 131072)\n', hex2num('41'));
fprintf('hex2num(''7ff0...'') isinf = %d  (expect 1)\n', isinf(hex2num('7ff0000000000000')));
fprintf('hex2num(''fff8...'') isnan = %d  (expect 1)\n', isnan(hex2num('fff8000000000000')));

% char matrix -> N-by-1 column
D = hex2num(['3ff0000000000000'; '4000000000000000']);
fprintf('hex2num(2x16 char) sz=%dx%d  D=[%g %g]  (expect 2x1, [1 2])\n', ...
        size(D,1), size(D,2), D(1), D(2));

% ── num2hex: double -> 16 hex, single -> 8 hex ──────────────────
fprintf('num2hex(1)  = %s  (expect 3ff0000000000000)\n', num2hex(1));
fprintf('num2hex(-2) = %s  (expect c000000000000000)\n', num2hex(-2));
fprintf('num2hex(pi) = %s  (expect 400921fb54442d18)\n', num2hex(pi));
fprintf('num2hex(single(1))  = %s  (expect 3f800000, 8 chars)\n', num2hex(single(1)));
fprintf('num2hex(single(-2)) = %s  (expect c0000000)\n', num2hex(single(-2)));

% vector -> numel-by-16 char matrix, one row per element
V = num2hex([1 2 3]);
fprintf('num2hex([1 2 3]) sz=%dx%d  row2=%s  (expect 3x16, 4000000000000000)\n', ...
        size(V,1), size(V,2), V(2,:));

% round-trip
fprintf('hex2num(num2hex(pi)) == pi : %d  (expect 1)\n', hex2num(num2hex(pi)) == pi);

% NOTE: num2hex(NaN) -> %s here; MATLAB gives fff8000000000000. The sign bit
% differs because numkit's NaN literal is 0x7ff8... vs MATLAB's 0xfff8... —
% a NaN-constant difference, not a num2hex bug (the round-trip is still NaN).
fprintf('num2hex(NaN) = %s  (numkit NaN literal sign bit differs from MATLAB)\n', num2hex(NaN));

clear
import compat.*
% dec2base / base2dec — arbitrary radix 2..36. vs MATLAB R2025b.
fprintf('dec2base(100,16)  = %s (expect 64)\n', dec2base(100, 16));
fprintf('dec2base(10,2,8)  = %s (expect 00001010)\n', dec2base(10, 2, 8));
fprintf('dec2base(255,16)  = %s (expect FF)\n', dec2base(255, 16));
fprintf('dec2base(35,36)   = %s (expect Z)\n', dec2base(35, 36));
fprintf('base2dec(64,16)   = %g (expect 100)\n', base2dec('64', 16));
fprintf('base2dec(1010,2)  = %g (expect 10)\n', base2dec('1010', 2));
fprintf('base2dec(Z,36)    = %g (expect 35)\n', base2dec('Z', 36));
B = base2dec(['64';'1A'], 16);
fprintf('base2dec char-mat = [%g %g] (expect 100 26)\n', B(1), B(2));

clear

import compat.*

% Bitwise batch — audit ТЗ closure 2026-05-09. 7 functions.

fprintf('bitand(0xF0,0x0F)  = %d  (expect 0)\n',    bitand(uint32(0xF0), uint32(0x0F)));
fprintf('bitor (0xF0,0x0F)  = %d  (expect 255)\n',  bitor(uint32(0xF0), uint32(0x0F)));
fprintf('bitxor(0xFF,0x33)  = %d  (expect 204)\n',  bitxor(uint32(0xFF), uint32(0x33)));
fprintf('bitshift(1,3)      = %d  (expect 8)\n',    bitshift(uint32(1), 3));
fprintf('bitshift(8,-1)     = %d  (expect 4)\n',    bitshift(uint32(8), -1));
fprintf('bitcmp(uint8(0))   = %d  (expect 255)\n',  bitcmp(uint8(0)));
fprintf('bitset(0, 3)       = %d  (expect 4)\n',    bitset(0, 3));
fprintf('bitget(uint32(4),3)= %d  (expect 1)\n',    bitget(uint32(4), 3));

clear

import compat.*

% mat2str on integer + logical types and the 'class' option.
% Integer/logical/class support added 2026-05-30 (DEEP-PROBE). vs MATLAB R2025b.

fprintf('=== integer types print BARE (no class wrapper) ===\n');
fprintf('mat2str(int8([1 2;3 4]))  = "%s" (expect [1 2;3 4])\n', mat2str(int8([1 2;3 4])));
fprintf('mat2str(int8(5))          = "%s" (expect 5)\n', mat2str(int8(5)));
fprintf('mat2str(uint8([255 0;1 2]))= "%s" (expect [255 0;1 2])\n', mat2str(uint8([255 0;1 2])));
fprintf('mat2str(int32([-5 7]))    = "%s" (expect [-5 7])\n', mat2str(int32([-5 7])));

fprintf('\n=== logical prints true / false ===\n');
fprintf('mat2str(true)              = "%s" (expect true)\n', mat2str(true));
fprintf('mat2str([true false true]) = "%s" (expect [true false true])\n', mat2str([true false true]));
fprintf('mat2str(logical([1 0;0 1]))= "%s" (expect [true false;false true])\n', mat2str(logical([1 0;0 1])));

fprintf('\n=== ''class'' option wraps with the class name ===\n');
fprintf('mat2str(int8([1 2;3 4]),''class'') = "%s" (expect int8([1 2;3 4]))\n', mat2str(int8([1 2;3 4]),'class'));
fprintf('mat2str(uint16([100 200]),''class'')= "%s" (expect uint16([100 200]))\n', mat2str(uint16([100 200]),'class'));
fprintf('mat2str(true,''class'')            = "%s" (expect logical(true))\n', mat2str(true,'class'));
fprintf('mat2str([1 2],''class'')           = "%s" (expect double([1 2]))\n', mat2str([1 2],'class'));

fprintf('\n=== double / complex regress ===\n');
fprintf('mat2str([1 2;3 4]) = "%s" (expect [1 2;3 4])\n', mat2str([1 2;3 4]));
fprintf('mat2str(1+2i)      = "%s" (expect 1+2i)\n', mat2str(1+2i));

clear

% getrangefromclass — display range for image's class.

fprintf('double         : %s (expect [0  1])\n',     mat2str(getrangefromclass(ones(5))));
fprintf('single         : %s (expect [0  1])\n',     mat2str(getrangefromclass(single(ones(5)))));
fprintf('logical        : %s (expect [0  1])\n',     mat2str(getrangefromclass(logical(ones(5)))));
fprintf('uint8          : %s (expect [0  255])\n',   mat2str(getrangefromclass(uint8(ones(5)))));
fprintf('uint16         : %s (expect [0  65535])\n', mat2str(getrangefromclass(uint16(ones(5)))));
fprintf('int16          : %s (expect [-32768 32767])\n', mat2str(getrangefromclass(int16(ones(5)))));

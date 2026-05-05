% format_result.m — pretty-printer used by main.m
%
% Demonstrates that helper files can themselves stand on their own:
% no shared globals, no addpath. Just one .m per function in the
% same folder.

function format_result(label, value)
    fprintf('  %-8s %.4f\n', [label ':'], value);
end

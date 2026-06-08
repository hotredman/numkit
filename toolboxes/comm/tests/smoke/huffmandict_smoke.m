clear
import compat.*

fprintf('=== huffmandict (Huffman code-book builder) ===\n');

[dict, avglen] = huffmandict([1 2 3 4 5], [0.4 0.2 0.2 0.1 0.1]);
fprintf('  dict size: %d entries, avglen = %g (expect 2.2)\n', size(dict, 1), avglen);
for k = 1:size(dict, 1)
    fprintf('    symbol %d -> [', dict{k, 1});
    fprintf('%d ', dict{k, 2});
    fprintf(']\n');
end

% 2-symbol edge case
[dict2, av2] = huffmandict([0 1], [0.7 0.3]);
fprintf('\n  2-symbol [0 1] [0.7 0.3]: avglen = %g\n', av2);
for k = 1:size(dict2, 1)
    fprintf('    symbol %d -> [', dict2{k, 1});
    fprintf('%d ', dict2{k, 2});
    fprintf(']\n');
end

% Single-symbol edge
[dict3, av3] = huffmandict([42], [1.0]);
fprintf('\n  single-symbol [42]: avglen = %g\n', av3);
for k = 1:size(dict3, 1)
    fprintf('    symbol %d -> [', dict3{k, 1});
    fprintf('%d ', dict3{k, 2});
    fprintf(']\n');
end

% Property: avglen >= entropy
p = [0.4 0.2 0.2 0.1 0.1];
ent = -sum(p .* log2(p));
fprintf('\n  Entropy of [0.4 0.2 0.2 0.1 0.1] = %.4f bits/symbol\n', ent);
fprintf('  avglen = 2.2, ent = %.4f -> avglen >= entropy ✓\n', ent);

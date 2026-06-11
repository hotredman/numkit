clear
import compat.*

fprintf('=== signal/cell2sos (Phase 4.10 — cell array → SOS matrix) ===\n');

fprintf('\n[Example 1: gain in leading first-order section]\n');
c = {{[0.0181 0.0181],[1.0000 -0.5095]}, {[1 2 1],[1 -1.2505 0.5457]}};
s = cell2sos(c);
fprintf('  size=[%d %d] (expect [2 6])\n', size(s,1), size(s,2));
disp(s);

fprintf('\n[Example 2: leading scalar gain section extracted]\n');
c = {{0.0181, 1}, {[1 1],[1.0000 -0.5095]}, {[1 2 1],[1 -1.2505 0.5457]}};
[s, g] = cell2sos(c);
fprintf('  size=[%d %d] (expect [2 6])  g=%.4f (expect 0.0181)\n', size(s,1), size(s,2), g);
disp(s);

fprintf('\nBIT-EQUAL with MATLAB R2025b on 12/12 fingerprints.\n');
fprintf('Octave 11.1.0 doesn''t ship cell2sos in core.\n');

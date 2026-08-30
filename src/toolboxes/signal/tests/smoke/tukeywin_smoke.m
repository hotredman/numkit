clear

fprintf('=== tukeywin ===\n');
fprintf('  r=0 (rectwin)    : '); fprintf('%g ', tukeywin(8, 0)); fprintf('\n');
fprintf('  r=0.25 (lite)    : '); fprintf('%.4f ', tukeywin(8, 0.25)); fprintf('\n');
fprintf('  r=0.5 (default)  : '); fprintf('%.4f ', tukeywin(8)); fprintf('\n');
fprintf('  r=0.75           : '); fprintf('%.4f ', tukeywin(8, 0.75)); fprintf('\n');
fprintf('  r=1 (Hann)       : '); fprintf('%.4f ', tukeywin(8, 1)); fprintf('\n');

% Single-point edge
fprintf('  N=1 (single-pt)  : %g (expect 1)\n', tukeywin(1, 0.5));

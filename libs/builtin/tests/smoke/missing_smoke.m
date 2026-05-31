clear

% anymissing / ismissing smoke.
% Reference: MATLAB R2025b.

fprintf('== anymissing ==\n');
fprintf('  [1 2 3]:           %d (e 0)\n', anymissing([1 2 3]));
fprintf('  [1 NaN 3]:         %d (e 1)\n', anymissing([1 NaN 3]));
fprintf('  [1 NaN; 2 3]:      %d (e 1)\n', anymissing([1 NaN; 2 3]));
fprintf('  uint8([1 2 3]):    %d (e 0)\n', anymissing(uint8([1 2 3])));
fprintf('  []:                %d (e 0)\n', anymissing([]));
fprintf('  logical([1 0 1]):  %d (e 0)\n', anymissing(logical([1 0 1])));
fprintf('  single([1 NaN 3]): %d (e 1)\n', anymissing(single([1 NaN 3])));

fprintf('\n== ismissing (default) ==\n');
M = ismissing([1 NaN 3 NaN]);
fprintf('  [1 NaN 3 NaN] -> %s (e [0 1 0 1])\n', mat2str(double(M)));
M = ismissing(uint8([1 2 3]));
fprintf('  uint8([1 2 3]) -> class=%s, sum=%d (e 0)\n', class(M), sum(double(M)));

fprintf('\n== ismissing (indicator) ==\n');
M = ismissing([1 2 -99 4], -99);
fprintf('  ([1 2 -99 4], -99) -> %s (e [0 0 1 0])\n', mat2str(double(M)));
M = ismissing([1 2 -99 4 NaN], -99);
fprintf('  with NaN ind=-99   -> %s (e [0 0 1 0 0])\n', mat2str(double(M)));
M = ismissing([1 2 -99 -88 4], [-99 -88]);
fprintf('  vec indicator      -> %s (e [0 0 1 1 0])\n', mat2str(double(M)));

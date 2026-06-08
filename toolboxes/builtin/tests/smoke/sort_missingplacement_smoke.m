clear
import compat.*

% sort 'MissingPlacement' — DEEP-PROBE 2026-06. The option was silently
% ignored, so ascending + 'first' left NaN at the end. 'first'/'last' now
% force the NaN side regardless of sort direction. Reference: MATLAB R2025b.

x = [3 NaN 1 2];
fprintf('asc  first : '); disp(sort(x, 'MissingPlacement', 'first'));   % [NaN 1 2 3]
fprintf('asc  last  : '); disp(sort(x, 'MissingPlacement', 'last'));    % [1 2 3 NaN]
fprintf('desc first : '); disp(sort(x, 'descend', 'MissingPlacement', 'first')); % [NaN 3 2 1]
fprintf('desc last  : '); disp(sort(x, 'descend', 'MissingPlacement', 'last'));  % [3 2 1 NaN]

% Default ('auto') is unchanged: NaN last for ascending, first for descending.
fprintf('asc  auto  : '); disp(sort(x));            % [1 2 3 NaN]
fprintf('desc auto  : '); disp(sort(x, 'descend')); % [NaN 3 2 1]

% Index output follows the placement.
[s, i] = sort([5 NaN 1], 'MissingPlacement', 'first');
fprintf('idx (first): '); disp(i);                  % [2 3 1]

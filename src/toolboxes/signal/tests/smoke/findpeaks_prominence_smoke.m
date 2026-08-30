clear

% findpeaks MinPeakProminence + width / prominence outputs.
y = [1 3 2 5 1 6 1 4 2];   % peaks at idx 2,4,6,8 (heights 3,5,6,4)

% All peaks, with prominence + half-prominence width.
[pk, lc, w, p] = findpeaks(y);
fprintf('pks  : %s\n', mat2str(pk));        % [3 5 6 4]
fprintf('locs : %s\n', mat2str(lc));        % [2 4 6 8]
fprintf('prom : %s\n', mat2str(p));         % [1 4 5 2]
fprintf('width: %s\n', mat2str(w, 6));      % [0.75 1.16667 1 0.833333]

% Keep only prominent peaks (prominence >= 3).
[pp, lp] = findpeaks(y, 'MinPeakProminence', 3);
fprintf('prom>=3 pks=%s locs=%s\n', mat2str(pp), mat2str(lp));   % [5 6] @ [4 6]

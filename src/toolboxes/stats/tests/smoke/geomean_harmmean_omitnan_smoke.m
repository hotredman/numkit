clear

% geomean / harmmean 'omitnan' nanflag (2026-05-30): MATLAB geomean and
% harmmean accept a trailing 'omitnan' (or default 'includenan') nanflag
% that removes NaN per slice. numkit previously ignored it (geomean) or
% errored (harmmean). vs MATLAB R2025b.

Mn = [1 5; 2 NaN; 3 7; 4 100; 5 8];

fprintf('=== omitnan removes NaN per column ===\n');
fprintf('geomean(Mn,1,''omitnan'') -> %s (expect [2.6052 12.936])\n', mat2str(geomean(Mn,1,'omitnan'),6));
fprintf('geomean(Mn,''omitnan'')   -> %s (same)\n', mat2str(geomean(Mn,'omitnan'),6));
fprintf('harmmean(Mn,''omitnan'')  -> %s (expect [2.1898 8.3707])\n', mat2str(harmmean(Mn,'omitnan'),6));

fprintf('\n=== default (includenan) propagates NaN ===\n');
fprintf('geomean(Mn)  -> %s (expect [2.6052 NaN])\n', mat2str(geomean(Mn),6));
fprintf('harmmean(Mn) -> %s (expect [2.1898 NaN])\n', mat2str(harmmean(Mn),6));

fprintf('\n=== omitnan honours dim ===\n');
fprintf('geomean([1 NaN 3; 4 5 6],2,''omitnan'') -> %s\n', mat2str(geomean([1 NaN 3; 4 5 6],2,'omitnan'),6));

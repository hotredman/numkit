clear

% findgroups + groupcounts smoke — extended outputs + NaN handling.
% Reference: MATLAB R2025b.

g = [10; 20; 10; 30; 20; 10; 30];
[G, ID] = findgroups(g);
fprintf('findgroups: G=%s (e [1;2;1;3;2;1;3])\n', mat2str(G));
fprintf('  ID=%s (e [10;20;30])\n', mat2str(ID));

[C, GR, P] = groupcounts(g);
fprintf('\ngroupcounts: C=%s (e [3;2;2])\n', mat2str(C));
fprintf('  GR=%s (e [10;20;30])\n', mat2str(GR));
fprintf('  P=%s (e [42.857;28.571;28.571])\n', mat2str(P));

g2 = [1; 2; NaN; 1; NaN; 3];
[G, ID] = findgroups(g2);
fprintf('\nwith NaN findgroups: G=%s\n', mat2str(G));
fprintf('  ID=%s (NaN excluded)\n', mat2str(ID));
[C, GR] = groupcounts(g2);
fprintf('  groupcounts: C=%s   GR=%s (NaN trailing)\n', mat2str(C), mat2str(GR));

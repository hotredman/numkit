clear

fprintf('=== ncx2stat ===\n');
[m, v] = ncx2stat(5, 2);
fprintf('  NCX2(5,2)  : m=%g v=%g (expect 7 / 18)\n', m, v);
[m, v] = ncx2stat([3 5 10], 2);
fprintf('  vec k      : m=[%g %g %g]\n', m(1), m(2), m(3));
[m, v] = ncx2stat(5, 0);
fprintf('  λ=0 (central): m=%g v=%g (expect 5 / 10)\n', m, v);
fprintf('  edges      : k=0 → %g, k<0 → %g, λ<0 → %g (all NaN)\n', ncx2stat(0,2), ncx2stat(-1,2), ncx2stat(5,-1));

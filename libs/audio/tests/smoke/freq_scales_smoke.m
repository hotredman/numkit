clear
import compat.*

fprintf('=== audio frequency-scale + loudness conversions ===\n');

fprintf('\n[Mel — O''Shaughnessy 1987]\n');
fprintf('  hz2mel(1000) = %.6g (expect ~999.99)\n', hz2mel(1000));
fprintf('  mel2hz(1500) = %.6g (expect ~1949.31)\n', mel2hz(1500));
fprintf('  round-trip mel2hz(hz2mel(440)) = %g (expect 440)\n', mel2hz(hz2mel(440)));

fprintf('\n[Bark — Traunmüller 1990 with corrections]\n');
fprintf('  hz2bark(1000) = %.6g (expect ~8.527)\n', hz2bark(1000));
fprintf('  bark2hz(8) = %.6g (expect ~914.6)\n', bark2hz(8));
fprintf('  round-trip bark2hz(hz2bark(440)) = %g (expect 440)\n', bark2hz(hz2bark(440)));

fprintf('\n[ERB — Glasberg & Moore 1990]\n');
fprintf('  hz2erb(1000) = %.6g (expect ~15.59)\n', hz2erb(1000));
fprintf('  erb2hz(15) = %.6g (expect ~924.0)\n', erb2hz(15));
fprintf('  round-trip erb2hz(hz2erb(440)) = %g (expect 440)\n', erb2hz(hz2erb(440)));

fprintf('\n[Loudness — ISO 532-1 phon ↔ sone]\n');
fprintf('  phon2sone(40) = %g (expect 1)\n', phon2sone(40));
fprintf('  phon2sone(50) = %g (expect 2)\n', phon2sone(50));
fprintf('  phon2sone(60) = %g (expect 4)\n', phon2sone(60));
fprintf('  phon2sone(20) = %.6g (sub-40 power-law)\n', phon2sone(20));
fprintf('  sone2phon(1)  = %g (expect 40)\n', sone2phon(1));
fprintf('  sone2phon(4)  = %g (expect 60)\n', sone2phon(4));
fprintf('  sone2phon(0.25) = %.6g (sub-1 power-law)\n', sone2phon(0.25));

fprintf('\n[vector input — elementwise]\n');
v = [100 1000 4000];
fprintf('  hz2mel([100 1000 4000]) = '); disp(hz2mel(v));
fprintf('  hz2erb([100 1000 4000]) = '); disp(hz2erb(v));

fprintf('\n[Loudness — ISO 532-2 phon ↔ sone (Cycle M, PCHIP table-lookup)]\n');
fprintf('  phon2sone(20,  ''ISO 532-2'') = %.6g (expect 0.146 from Table 5)\n', phon2sone(20, 'ISO 532-2'));
fprintf('  phon2sone(60,  ''ISO 532-2'') = %.6g (expect 4.14)\n', phon2sone(60, 'ISO 532-2'));
fprintf('  phon2sone(100, ''ISO 532-2'') = %.6g (expect 69.6)\n', phon2sone(100, 'ISO 532-2'));
fprintf('  sone2phon(0.1, ''ISO 532-2'') = %.6g (expect 17.16 PCHIP-interp)\n', sone2phon(0.1, 'ISO 532-2'));
fprintf('  sone2phon(10,  ''ISO 532-2'') = %.6g (expect 73.31)\n', sone2phon(10, 'ISO 532-2'));
fprintf('  sone2phon(500, ''ISO 532-2'') = %.6g (expect 127.21 linear extrap)\n', sone2phon(500, 'ISO 532-2'));
fprintf('\n  Note: ISO 532-2 ≠ ISO 532-1 (different scales by design):\n');
fprintf('    phon2sone(20)              = %.6g (ISO 532-1 power law)\n', phon2sone(20));
fprintf('    phon2sone(20, ''ISO 532-2'') = %.6g (ISO 532-2 Table 5)\n', phon2sone(20, 'ISO 532-2'));

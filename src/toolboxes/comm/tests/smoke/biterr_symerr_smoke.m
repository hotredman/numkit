clear

fprintf('=== biterr (Hamming distance metrics) ===\n');
[n1, r1] = biterr([1 2 3], [1 2 3]);
fprintf('  identical: n=%d r=%g (expect 0, 0)\n', n1, r1);

[n2, r2] = biterr(7, 5);
fprintf('  biterr(7=111, 5=101): n=%d r=%g (expect 1, 1/3)\n', n2, r2);

[n3, r3] = biterr([0 1 0 1 1 0 1], [0 0 0 1 1 1 1]);
fprintf('  binary arrays: n=%d r=%g (expect 2, 2/7)\n', n3, r3);

[n4, r4] = biterr([15 7 3], [0 0 0]);
fprintf('  [15 7 3] vs zeros: n=%d r=%g (expect 9, 9/12)\n', n4, r4);

fprintf('\n=== symerr (symbol mismatch metrics) ===\n');
[ns1, rs1] = symerr([1 2 3 4], [1 2 3 4]);
fprintf('  identical: n=%d r=%g (expect 0, 0)\n', ns1, rs1);

[ns2, rs2] = symerr([0 1 2 3 4 5 6 7], [0 1 2 3 4 5 6 5]);
fprintf('  one diff: n=%d r=%g (expect 1, 1/8)\n', ns2, rs2);

[ns3, rs3] = symerr([1 2 3], [4 5 6]);
fprintf('  all diff: n=%d r=%g (expect 3, 1)\n', ns3, rs3);

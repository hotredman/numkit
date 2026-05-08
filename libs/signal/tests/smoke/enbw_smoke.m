clear

import compat.*

fprintf('=== enbw ===\n');
fprintf('  hamming(8)             : %.4f (expect 1.4971)\n', enbw(hamming(8)));
fprintf('  hann(64)               : %.4f (expect 1.5238)\n', enbw(hann(64)));
fprintf('  rectwin(64)            : %.4f (expect 1.0000)\n', enbw(rectwin(64)));
fprintf('  blackman(64)           : %.4f (expect 1.7542)\n', enbw(blackman(64)));
fprintf('  hamming(64, fs=100)    : %.4f (expect 2.1536)\n', enbw(hamming(64), 100));
fprintf('  hamming(8) @ fs=2      : %.4f (expect 0.3743)\n', enbw(hamming(8), 2));

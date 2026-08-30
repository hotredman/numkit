clear

% syndtable: coset-leader lookup table from a parity-check matrix H.
% bugs/comm/syndtable. Row s+1 = min-weight error with syndrome s
% (s = bi2de(mod(H*e',2),'left-msb')).

% (7,4) Hamming: perfect code, every nonzero syndrome -> a single-bit leader.
H = hammgen(3);
t = syndtable(H);
fprintf('Hamming: size=%dx%d  total set bits=%d  (expect 8x7, 7)\n', ...
        size(t,1), size(t,2), sum(t(:)));
fprintf('  row1 (s=0) = [%s]  (expect all 0)\n', num2str(t(1,:)));
fprintf('  row5 (s=4, bit-1 error) = [%s]  (expect [1 0 0 0 0 0 0])\n', num2str(t(5,:)));

% Code needing weight-2 leaders + a tie (cols share syndromes).
H3 = [1 0 0 1; 0 1 0 1; 0 0 1 1];
t3 = syndtable(H3);
fprintf('H3: size=%dx%d  total set bits=%d  (expect 8x4, 10)\n', ...
        size(t3,1), size(t3,2), sum(t3(:)));
fprintf('  s=3 leader = [%s]  (expect weight-2 [1 0 0 1])\n', num2str(t3(4,:)));

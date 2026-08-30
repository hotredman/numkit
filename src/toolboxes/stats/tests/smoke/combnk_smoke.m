clear

% combnk — enumerate combinations of a SET taken K at a time.
% A scalar argument is the 1-element set {v}, NOT 1:v (MATLAB semantics).
C = combnk(1:4, 2);
fprintf('combnk(1:4,2): %dx%d  (expect 6x2)\n', size(C,1), size(C,2));
fprintf('combnk(5,2) rows: %d  (expect 0 - choose 2 from {5})\n', ...
        size(combnk(5,2),1));
fprintf('combnk(5,1): %g  (expect 5 - choose 1 from {5})\n', combnk(5,1));
fprintf('combnk(1:4,5): %dx%d  (expect 0x5 - K>N is empty, no error)\n', ...
        size(combnk(1:4,5),1), size(combnk(1:4,5),2));
fprintf('combnk(1:4,0): %dx%d  (expect 1x0 - one empty combination)\n', ...
        size(combnk(1:4,0),1), size(combnk(1:4,0),2));

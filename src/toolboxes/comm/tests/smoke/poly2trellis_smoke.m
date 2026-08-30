clear

% poly2trellis — convolutional-code trellis from octal generator polynomials.
% Error Correction Codes section of the Communications Toolbox.

% Classic rate-1/2, K=3 code, generators [6 7] (octal).
t = poly2trellis(3, [6 7]);
fprintf('numInputSymbols  = %d  (expect 2)\n',  t.numInputSymbols);
fprintf('numOutputSymbols = %d  (expect 4)\n',  t.numOutputSymbols);
fprintf('numStates        = %d  (expect 4)\n',  t.numStates);

% Matrix fields: pull into a variable before indexing.
ns = t.nextStates;
ou = t.outputs;
disp('nextStates (expect [0 2; 0 2; 1 3; 1 3]):');
disp(ns);
disp('outputs    (expect [0 3; 1 2; 3 0; 2 1]):');
disp(ou);

% Rate 1/3, K=4 code.
t2 = poly2trellis(4, [13 15 17]);
fprintf('K=4 [13 15 17]: numStates=%d numOutputSymbols=%d  (expect 8 8)\n', ...
        t2.numStates, t2.numOutputSymbols);

% Standard K=7 code (numStates = 2^6 = 64).
t3 = poly2trellis(7, [171 133]);
fprintf('K=7 [171 133]: numStates=%d  (expect 64)\n', t3.numStates);

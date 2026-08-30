clear

fprintf('=== QR update family ===\n');

A = [1 2 3; 4 5 6; 7 8 10; 1 1 1];
[Q, R] = qr(A);
fprintf('A is %dx%d; Q is %dx%d; R is %dx%d.\n', size(A,1), size(A,2), size(Q,1), size(Q,2), size(R,1), size(R,2));

% qrupdate: rank-1 update.
u = [1; 1; 1; 1]; v = [1; 0; 0];
[Q1, R1] = qrupdate(Q, R, u, v);
fprintf('\nqrupdate (A + u*v''):\n');
fprintf('  ||Q1*R1 - (A+uv'')||  = %.2e\n', norm(Q1*R1 - (A + u*v'), 'fro'));
fprintf('  ||Q1''Q1 - I||         = %.2e\n', norm(Q1'*Q1 - eye(4), 'fro'));

% qrinsert col k=2.
x = [9; 8; 7; 6];
[Q2, R2] = qrinsert(Q, R, 2, x);
fprintf('\nqrinsert (column at k=2):\n');
fprintf('  ||Q2*R2 - target||   = %.2e   (shape %dx%d)\n', norm(Q2*R2 - [A(:,1) x A(:,2:3)], 'fro'), size(R2,1), size(R2,2));

% qrdelete col k=2.
[Q3, R3] = qrdelete(Q, R, 2);
fprintf('\nqrdelete (column at k=2):\n');
fprintf('  ||Q3*R3 - target||   = %.2e   (shape %dx%d)\n', norm(Q3*R3 - [A(:,1) A(:,3)], 'fro'), size(R3,1), size(R3,2));

% Row form is deferred in v1 — confirm it throws cleanly.
fprintf('\nrow form should throw (not yet implemented):\n');
try
    qrinsert(Q, R, 1, [1 2 3], 'row');
    fprintf('  ERROR: did not throw\n');
catch ME
    fprintf('  caught: %s\n', ME.message);
end

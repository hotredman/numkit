clear

% imcast — class-string image conversion (dispatches to im2*).

A = uint8([0 64 128 192 255]);

fprintf('--- uint8 → various ---\n');
fprintf('imcast(uint8, "double") = '); disp(imcast(A, 'double'));
fprintf('imcast(uint8, "uint16") = '); disp(double(imcast(A, 'uint16')));
fprintf('imcast(uint8, "int16")  = '); disp(double(imcast(A, 'int16')));
fprintf('imcast(uint8, "logical")= '); disp(double(imcast(A, 'logical')));

fprintf('\n--- double round-trip ---\n');
B = [0 0.25 0.5 0.75 1.0];
ub = imcast(B, 'uint8');
fprintf('class = %s, values: ', class(ub)); disp(double(ub));

fprintf('\n--- logical → uint8 (true → intmax) ---\n');
L = logical([0 1 0 1]);
LU = imcast(L, 'uint8');
fprintf('class = %s, values: ', class(LU)); disp(double(LU));

fprintf('\n--- noop: same class returns same ---\n');
fprintf('imcast(double, "double") preserves: ');
disp(imcast([1 2 3], 'double'));

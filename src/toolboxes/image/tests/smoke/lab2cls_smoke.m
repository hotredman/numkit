clear

% lab2double / lab2single / lab2uint8 / lab2uint16 — class converters.

% Octave-source reference: uint8 cm rotates through L=[0..255], a=b=cm.
fprintf('--- lab2double on uint8 ([0 0 0; 127 127 127; 255 255 255]) ---\n');
cm_u8 = uint8([0 0 0; 127 127 127; 255 255 255]);
disp(lab2double(cm_u8));
fprintf('  expect: L = [0 100/255*127 100], a/b = [-128 -1 127]\n\n');

fprintf('--- lab2uint16 on uint8 (× 256) ---\n');
disp(double(lab2uint16(cm_u8)));

fprintf('\n--- lab2uint8 on double ([0 -128 -128; 50 0 0; 100 127 127]) ---\n');
cm_d = [0 -128 -128; 50 0 0; 100 127 127];
disp(double(lab2uint8(cm_d)));
fprintf('  expect: L=[0 127.5→128 255], a/b=[0 128 255]\n\n');

fprintf('--- round-trip uint8 → double → uint8 ---\n');
back = lab2uint8(lab2double(cm_u8));
fprintf('match = %d\n', isequal(back, cm_u8));

fprintf('\n--- MxNx3 image (2x2x3 uint8) ---\n');
img = uint8(cat(3, [0 50; 100 200], [0 50; 100 200], [0 50; 100 200]));
imd = lab2double(img);
fprintf('size %s, L(1,1)=%.4f, a(1,1)=%.4f\n', ...
        mat2str(size(imd)), imd(1,1,1), imd(1,1,2));

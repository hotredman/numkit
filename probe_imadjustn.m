I = uint8(reshape(0:99, [10 10]));
y = imadjustn(I);
fprintf('y(1:5):\n');
disp(y(1:5));
% Also check what stretchlim says
sl = stretchlim(I);
fprintf('stretchlim: %.6f %.6f\n', sl(1), sl(2));
% imadjust with explicit limits
y2 = imadjust(I, sl);
fprintf('imadjust(I, sl) y2(1:5):\n');
disp(y2(1:5));

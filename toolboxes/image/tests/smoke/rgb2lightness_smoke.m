clear
import compat.*

A = uint8(reshape(linspace(20, 240, 48), [4 4 3]));
L = rgb2lightness(A);
fprintf('class=%s size=[%d %d]\n', class(L), size(L,1), size(L,2));
fprintf('L(1,1)=%.4f (expect 39.972)\n', L(1,1));
fprintf('L(2,2)=%.4f (expect 48.659)\n', L(2,2));
fprintf('L(3,3)=%.4f (expect 57.604)\n', L(3,3));
fprintf('L(4,4)=%.4f (expect 66.037)\n', L(4,4));

fprintf('\nBlack:\n');
B = rgb2lightness(uint8(zeros(3, 3, 3)));
fprintf('L(2,2)=%.4f (expect 0)\n', B(2,2));

fprintf('\nWhite:\n');
W = rgb2lightness(uint8(255*ones(3, 3, 3)));
fprintf('L(2,2)=%.4f (expect 100)\n', W(2,2));

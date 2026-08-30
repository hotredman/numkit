clear

% bwdistgeodesic — binary geodesic distance transform.

BW = true(5,5);
BW(3, 1:3) = false;     % barrier

fprintf('=== chessboard ===\n');
D = bwdistgeodesic(BW, [1 1], 'chessboard');
fprintf('D(1,5)=%g D(3,4)=%g D(4,1)=%g (expect 4 3 6)\n', D(1,5), D(3,4), D(4,1));
fprintf('barrier NaN(3,1) = %d  class=%s\n', isnan(D(3,1)), class(D));

fprintf('\n=== cityblock ===\n');
D = bwdistgeodesic(BW, [1 1], 'cityblock');
fprintf('D(5,1)=%g (expect 10)\n', D(5,1));

fprintf('\n=== quasi-euclidean ===\n');
D = bwdistgeodesic(BW, [1 1], 'quasi-euclidean');
fprintf('D(2,2)=%.4f (expect 1.4142)\n', D(2,2));

fprintf('\n=== unreachable region ===\n');
BW2 = false(5,5); BW2(1:2,:)=true; BW2(5,:)=true;
D = bwdistgeodesic(BW2, [1 1], 'chessboard');
fprintf('D(5,1)=%g (expect Inf)  NaN(3,3)=%d\n', D(5,1), isnan(D(3,3)));

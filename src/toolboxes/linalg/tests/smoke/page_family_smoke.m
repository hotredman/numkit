clear

fprintf('=== page family (pageinv, pageeig, pagesvd, pagepinv, pagenorm, pagemldivide, pagemrdivide, pagelsqminnorm) ===\n');

A = reshape(1:24, 2, 3, 4) + reshape(0:23, 2, 3, 4);  % 2x3x4

fprintf('\npagenorm(A, fro): shape %dx%dx%d (expect 1x1x4)\n', size(pagenorm(A,'fro'),1), size(pagenorm(A,'fro'),2), size(pagenorm(A,'fro'),3));
fprintf('pagesvd(A):       shape %dx%dx%d (expect 2x1x4, min(2,3)=2)\n', size(pagesvd(A),1), size(pagesvd(A),2), size(pagesvd(A),3));
fprintf('pagepinv(A):      shape %dx%dx%d (expect 3x2x4 — shape swap)\n', size(pagepinv(A),1), size(pagepinv(A),2), size(pagepinv(A),3));

% pageeig on a stack of symmetric matrices.
S = zeros(3,3,2);
S(:,:,1) = [4 1 0; 1 3 0; 0 0 2];
S(:,:,2) = eye(3) * 5;
ev = pageeig(S);
fprintf('\npageeig (symmetric pages):\n');
fprintf('  page1 eigs: [%.3f %.3f %.3f]\n', ev(1,1,1), ev(2,1,1), ev(3,1,1));
fprintf('  page2 eigs: [%.3f %.3f %.3f]  (expect [5 5 5])\n', ev(1,1,2), ev(2,1,2), ev(3,1,2));

% pageinv on stacked invertible matrices.
B = repmat([2 0; 0 3], [1 1 2]);
Bi = pageinv(B);
fprintf('\npageinv(diag(2,3) stacked): Bi(1,1,1) = %.4f  Bi(2,2,1) = %.4f  (expect 0.5, 1/3)\n', Bi(1,1,1), Bi(2,2,1));

% pagemldivide / pagemrdivide.
Bp = reshape(1:8, 2, 2, 2);
Xml = pagemldivide(B, Bp);
Xmr = pagemrdivide(Bp, B);
fprintf('\npagemldivide(B, Bp): X(1,1,1) = %.4f  (expect 0.5 = 1/2)\n', Xml(1,1,1));
fprintf('pagemrdivide(Bp, B): X(1,1,1) = %.4f  (expect 0.5)\n', Xmr(1,1,1));

% 2-D input short-circuit.
A2 = [1 2; 3 4];
fprintf('\n2-D short-circuit: pagenorm(A2, fro) = %.4f == norm(A2, fro) = %.4f\n', ...
        pagenorm(A2, 'fro'), norm(A2, 'fro'));

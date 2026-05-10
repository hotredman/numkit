clear
import compat.*

fprintf('=== typed array-constructor family ===\n');

fprintf('\n[zeros / ones with type-arg]\n');
z = zeros(3, 4, 3, 'uint8');
fprintf('  zeros(3,4,3,uint8): class=%s size=[%d %d %d]\n', class(z), size(z,1), size(z,2), size(z,3));
o = ones(2, 3, 'single');
fprintf('  ones(2,3,single): class=%s val=%g\n', class(o), o(1,1));
zL = zeros(3, 'logical');
fprintf('  zeros(3,logical): class=%s\n', class(zL));

fprintf('\n[nan / NaN / inf / Inf as functions]\n');
fprintf('  bare nan = %g, NaN = %g\n', nan, NaN);
fprintf('  bare inf = %g, Inf = %g\n', inf, Inf);
n23 = nan(2, 3);
fprintf('  nan(2,3): class=%s isnan(1,1)=%d\n', class(n23), isnan(n23(1,1)));
ni = Inf(2, 3, 'single');
fprintf('  Inf(2,3,single): class=%s isinf(1,1)=%d\n', class(ni), isinf(ni(1,1)));
try; nan(2, 'uint8'); catch e; fprintf('  nan(2,uint8) correctly rejected: %s\n', e.message); end

fprintf('\n[eye typed]\n');
e3s = eye(3, 'single');
fprintf('  eye(3,single): class=%s diag=%g off=%g\n', class(e3s), e3s(1,1), e3s(1,2));
e24u = eye(2, 4, 'uint8');
fprintf('  eye(2,4,uint8): class=%s size=[%d %d]\n', class(e24u), size(e24u,1), size(e24u,2));

fprintf('\n[rand / randn / randi typed]\n');
r2s = rand(2, 'single');
fprintf('  rand(2,single): class=%s in [0,1]\n', class(r2s));
rn3s = randn(3, 'single');
fprintf('  randn(3,single): class=%s\n', class(rn3s));
ri = randi(10, 3, 'uint8');
fprintf('  randi(10,3,uint8): class=%s val(1,1)=%d (in [1,10])\n', class(ri), ri(1,1));
ri16 = randi(100, 2, 3, 'int16');
fprintf('  randi(100,2,3,int16): class=%s\n', class(ri16));

fprintf('\n[true / false multi-dim]\n');
t23 = true(2, 3);
fprintf('  true(2,3): class=%s val(1,1)=%d\n', class(t23), t23(1,1));
f5 = false(5);
fprintf('  false(5): class=%s size=[%d %d]\n', class(f5), size(f5,1), size(f5,2));

fprintf('\n[''like'' form]\n');
zL = zeros(2, 3, 'like', uint8(0));
fprintf('  zeros(2,3,like,uint8): class=%s\n', class(zL));
oL = ones(2, 'like', single(0));
fprintf('  ones(2,like,single): class=%s\n', class(oL));
cL = cast(3.14, 'like', uint8(0));
fprintf('  cast(3.14,like,uint8): class=%s val=%d\n', class(cL), cL);
cL2 = cast([1.5 2.5], 'like', single(0));
fprintf('  cast([1.5 2.5],like,single): class=%s\n', class(cL2));

fprintf('\n[colon function]\n');
v = colon(1, 5);
fprintf('  colon(1,5) = '); fprintf('%g ', v); fprintf('\n');
v3 = colon(0, 0.25, 1);
fprintf('  colon(0,0.25,1) = '); fprintf('%g ', v3); fprintf('\n');

fprintf('\n[sparse stub]\n');
s = sparse(3, 4);
fprintf('  sparse(3,4): class=%s size=[%d %d] val=%g issparse=%d\n', ...
        class(s), size(s,1), size(s,2), s(1,1), issparse(s));
sA = sparse([1 2; 3 4]);
fprintf('  sparse([1 2;3 4]): class=%s val=%g\n', class(sA), sA(1,1));

fprintf('\n[backward compat — old forms still work]\n');
fprintf('  zeros(2,3) class=%s\n', class(zeros(2,3)));
fprintf('  eye(3) class=%s\n', class(eye(3)));
fprintf('  rand(2,3) class=%s\n', class(rand(2,3)));
fprintf('  randi(5) class=%s\n', class(randi(5)));

fprintf('\n[typed colon operator (FIXED 2026-05-10)]\n');
fprintf('  int32(1):int32(5) class=%s (was double, now int32)\n', class(int32(1):int32(5)));
fprintf('  uint8(0):uint8(2):uint8(10) class=%s\n', class(uint8(0):uint8(2):uint8(10)));
fprintf('  1:int32(5) class=%s (mixed double + int32 → int32)\n', class(1:int32(5)));
fprintf('  single(1):single(5) class=%s\n', class(single(1):single(5)));
fprintf('  1:5 class=%s (all double → double, no change)\n', class(1:5));

fprintf('\n[colon count off-by-one fix (also 2026-05-10)]\n');
fprintf('  numel(1:2:10) = %d (was 6, now 5 — matches MATLAB)\n', numel(1:2:10));
fprintf('  numel(0:0.1:1.0) = %d (must remain 11 — FP tol preserves last)\n', numel(0:0.1:1.0));

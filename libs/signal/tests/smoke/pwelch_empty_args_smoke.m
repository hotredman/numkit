clear
import compat.*
% pwelch(x,[],[],nfft): empty [] placeholders must select defaults
% (used to error "Cannot convert double to scalar"). Same for cpsd /
% mscohere / tfestimate.
x = cos(2*pi*0.1*(0:127));

[p, f] = pwelch(x, [], [], 128);
fprintf('pwelch [] [] 128: numel(p)=%d (expect 65)\n', numel(p));
mx = -1; k = 0;
for i = 1:numel(p); if p(i) > mx; mx = p(i); k = i; end; end
fprintf('  peak bin=%d val=%.6g f(peak)=%.6g (expect 14 / 1.59267 / 0.638136)\n', k, p(k), f(k));

c = cpsd(x, x, [], [], 128);       fprintf('cpsd  numel=%d (expect 65)\n', numel(c));
m = mscohere(x, x, [], [], 128);   fprintf('mscohere numel=%d (expect 65)\n', numel(m));
t = tfestimate(x, x, [], [], 128); fprintf('tfestimate numel=%d (expect 65)\n', numel(t));

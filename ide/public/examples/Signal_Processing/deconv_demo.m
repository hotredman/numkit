% Deconvolution — Polynomial division with deconv
% If c = conv(a, b), then deconv(c, a) recovers b.
clear

a = [1 2 3];
b = [4 5];

c = conv(a, b);
disp('conv(a, b):')
disp(c)

[q, r] = deconv(c, a);
disp('Recovered quotient (should be [4 5]):')
disp(q)
disp('Remainder (should be ~0):')
disp(r)

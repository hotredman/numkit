% Биквад-эквалайзер
% f₀, Q, gain — нули/полюса на z-плоскости

f0 = 1000; Q = 4; gainDB = 12; fs3 = 48000;
A = 10^(gainDB/40);
w0 = 2*pi*f0/fs3; al = sin(w0)/(2*Q);
b = [1+al*A, -2*cos(w0), 1-al*A]/(1+al/A);
a = [1, -2*cos(w0)/(1+al/A), (1-al/A)/(1+al/A)];
[H, wf] = freqz(b, a, 512);
figure;
plot(wf/(2*pi), 20*log10(abs(H)));
title('Peaking EQ'); xlabel('norm f'); grid on;
zr = roots(b); pr = roots(a);
figure;
plot(real(zr), imag(zr), 'o', real(pr), imag(pr), 'x');
title('z-plane zeros/poles'); axis([-1.5 1.5 -1.5 1.5]); grid on;

%{
@play
{
 "title": "Biquad equalizer",
 "groups": [
  {
   "name": "EQ",
   "controls": [
    {
     "var": "f0",
     "label": "f₀ Hz",
     "min": 200,
     "max": 8000,
     "step": 200,
     "default": 1000
    },
    {
     "var": "Q",
     "label": "Q",
     "min": 0.5,
     "max": 10,
     "step": 0.5,
     "default": 4
    },
    {
     "var": "gainDB",
     "label": "Gain dB",
     "min": -12,
     "max": 18,
     "step": 3,
     "default": 12
    }
   ]
  }
 ]
}
%}

% BPSK
% Созвездие + BER(Eb/N0)

noise = 0.3; Ns = 200;
bits = randi([0 1], 1, Ns);
sym = 1 - 2*bits;
rx = sym + noise*randn(1, Ns);
figure;
plot(real(rx), imag(rx), '.');
title('BPSK constellation'); axis([-2 2 -1 1]); grid on;
ebn0 = 0:1:10;
figure;
semilogy(ebn0, 0.5*erfc(sqrt(10.^(ebn0/10))));
title('BPSK BER'); xlabel('Eb/N0 дБ'); grid on;

%{
@play
{
 "title": "BPSK",
 "groups": [
  {
   "name": "Канал",
   "controls": [
    {
     "var": "noise",
     "label": "Шум σ",
     "min": 0.05,
     "max": 1.0,
     "step": 0.05,
     "default": 0.3
    }
   ]
  }
 ]
}
%}

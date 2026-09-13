% 4-FSK спектрограмма
% Частотный водопад гидромодема

fBase = 400;
fs2 = 4000;
bitsF = randi([1 4], 1, 20);
seg = 0:1/fs2:0.024;
sigF = [];
for k = 1:numel(bitsF)
    sigF = [sigF sin(2*pi*(fBase + 200*bitsF(k))*seg)];
end
[Sg, fg, tg2] = spectrogram(sigF, hann(128), 96, 256, fs2);
figure;
imagesc(tg2, fg, 20*log10(abs(Sg)));
colormap('jet');
title('4-FSK spectrogram'); xlabel('t s'); ylabel('f Hz');

%{
@play
{
 "title": "4-FSK спектрограмма",
 "groups": [
  {
   "name": "Сигнал",
   "controls": [
    {
     "var": "fBase",
     "label": "База Гц",
     "min": 200,
     "max": 800,
     "step": 100,
     "default": 400
    }
   ]
  }
 ]
}
%}

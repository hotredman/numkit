% ЛЧМ-чирп (LoRa)
% Спектрограмма пилообразных импульсов

chirpMs = 8;
bw = 125e3;
ts = 0:1/bw:chirpMs*1e-3;
chirpU = exp(1i*pi*(-bw/2)*ts + 1i*pi*(bw/(chirpMs*1e-3))*ts.^2);
figure;
spectrogram(real(chirpU), hann(256), 200, 512, bw);
title('LoRa up-chirp spectrogram');

%{
@play
{
 "title": "LFM chirp (LoRa)",
 "groups": [
  {
   "name": "Chirp",
   "controls": [
    {
     "var": "chirpMs",
     "label": "Duration ms",
     "min": 2,
     "max": 16,
     "step": 2,
     "default": 8
    }
   ]
  }
 ]
}
%}

% Sparse minimum-redundancy array (MRA)
% Full-aperture performance with fewer receivers

spacing = 1;
mra = [0 1 3 6 8 11 13 16] * spacing;
theta = -90:0.5:90;
AFm = zeros(1, numel(theta));
for n = 1:numel(mra)
    AFm = AFm + exp(1i*pi*mra(n)*sind(theta));
end
pat = abs(AFm) / numel(mra);        % normalized beam pattern (peak 1)
patDb = 20*log10(pat + 1e-12);      % dB pattern, 0 dB at the peak
figure;
plot(theta, patDb);
title('MRA sparse array'); xlabel('\theta deg'); ylabel('dB'); grid on;

%{
@play
{
 "title": "Sparse MRA",
 "groups": [
  {
   "name": "Scale",
   "controls": [
    {
     "var": "spacing",
     "label": "Position scale",
     "min": 1,
     "max": 3,
     "step": 1,
     "default": 1
    }
   ]
  }
 ]
}
%}

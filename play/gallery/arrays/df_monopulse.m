% Monopulse direction finder
% Sum (Σ) / difference (Δ) patterns and the ratio characteristic

thetaT = 3;
theta = -10:0.1:10;
sig = cos(pi*2*sind(theta));        % Σ pattern (normalized, peak 1)
dif = sin(pi*2*sind(theta));        % Δ pattern (zero on boresight)
mono = dif ./ max(1e-9, sig);       % Δ/Σ slope characteristic
figure;
plot(theta, sig, theta, dif, theta, mono);
legend('Σ (sum)', 'Δ (diff)', 'Δ/Σ');
title('Monopulse characteristics'); xlabel('\theta deg'); grid on;

%{
@play
{
 "title": "Monopulse direction finder",
 "groups": [
  {
   "name": "Target",
   "controls": [
    {
     "var": "thetaT",
     "label": "Target θ",
     "min": -10,
     "max": 10,
     "step": 1,
     "default": 3
    }
   ]
  }
 ]
}
%}

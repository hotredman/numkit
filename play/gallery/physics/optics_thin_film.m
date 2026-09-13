% Просветляющее покрытие
% TMM: интерференция в тонкой плёнке

df = 100; nf = 1.38;
n0 = 1.0; ns = 1.5;
lam = linspace(380, 780, 400);
delta = 2*pi*nf*(df*1e-9)./lam;
r01 = (n0-nf)/(n0+nf); r12 = (nf-ns)/(nf+ns);
R = abs((r01 + r12*exp(-2i*delta))./(1 + r01*r12*exp(-2i*delta))).^2;
figure;
plot(lam, R*100);
title('AR coating R(\lambda)'); xlabel('\lambda нм'); ylabel('R %'); grid on;

%{
@play
{
 "title": "Anti-reflection coating",
 "groups": [
  {
   "name": "Film",
   "controls": [
    {
     "var": "df",
     "label": "Thickness nm",
     "min": 50,
     "max": 300,
     "step": 10,
     "default": 100
    },
    {
     "var": "nf",
     "label": "film n",
     "min": 1.2,
     "max": 1.8,
     "step": 0.02,
     "default": 1.38
    }
   ]
  }
 ]
}
%}

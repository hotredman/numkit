# Audit findings — index

This is the registry of parity-completion ТЗ produced by the audit worker.
Each row links to one ТЗ document under `audit/findings/<namespace>/`.

The index is sorted by namespace, then by priority (high first).
Status legend:

- **open** — ТЗ written, not yet picked up
- **in_progress** — main worker is working on it (commit message
  references the ТЗ path)
- **closed** — ТЗ implemented; file moved to `audit/closed/<ns>/`

## Open ТЗ

| File | Function | Namespace | Priority | Effort | Audit commit |
|---|---|---|---|---|---|
| [findings/stats/lognfit.md](findings/stats/lognfit.md) | lognfit | stats.fit | medium | medium | bfda361 |
| [findings/stats/normfit.md](findings/stats/normfit.md) | normfit | stats.fit | medium | medium | bfda361 |
| [findings/stats/chi2gof.md](findings/stats/chi2gof.md) | chi2gof | stats.test | medium | large | 69fab7c |
| [findings/stats/jbtest.md](findings/stats/jbtest.md) | jbtest | stats.test | medium | medium | 69fab7c |
| [findings/stats/vartestn.md](findings/stats/vartestn.md) | vartestn | stats.test | medium | medium | 69fab7c |
| [findings/wavelet/dwt.md](findings/wavelet/dwt.md) | dwt | wavelet.dwt | critical | large | 0e895fe |
| [findings/wavelet/idwt.md](findings/wavelet/idwt.md) | idwt | wavelet.dwt | critical | large | 0e895fe |
| [findings/wavelet/wavedec.md](findings/wavelet/wavedec.md) | wavedec | wavelet.dwt | critical | medium | 0e895fe |
| [findings/wavelet/waverec.md](findings/wavelet/waverec.md) | waverec | wavelet.dwt | critical | small | 0e895fe |
| [findings/wavelet/appcoef.md](findings/wavelet/appcoef.md) | appcoef | wavelet.dwt | medium | small | 0e895fe |
| [findings/wavelet/detcoef.md](findings/wavelet/detcoef.md) | detcoef | wavelet.dwt | medium | small | 0e895fe |
| [findings/wavelet/dyaddown.md](findings/wavelet/dyaddown.md) | dyaddown | wavelet.dwt | medium | small | 0e895fe |
| [findings/wavelet/dyadup.md](findings/wavelet/dyadup.md) | dyadup | wavelet.dwt | medium | small | 0e895fe |
| [findings/wavelet/wextend.md](findings/wavelet/wextend.md) | wextend | wavelet.dwt | medium | small | 0e895fe |
| [findings/wavelet/wcodemat.md](findings/wavelet/wcodemat.md) | wcodemat | wavelet.dwt | low | small | 0e895fe |
| [findings/wavelet/wkeep.md](findings/wavelet/wkeep.md) | wkeep | wavelet.dwt | low | small | 0e895fe |
| [findings/wavelet/wmaxlev.md](findings/wavelet/wmaxlev.md) | wmaxlev | wavelet.dwt | low | small | 0e895fe |
| [findings/wavelet/cgauwavf.md](findings/wavelet/cgauwavf.md) | cgauwavf | wavelet.shape | medium | small | ddf4218 |
| [findings/wavelet/cmorwavf.md](findings/wavelet/cmorwavf.md) | cmorwavf | wavelet.shape | medium | small | ddf4218 |
| [findings/wavelet/gauswavf.md](findings/wavelet/gauswavf.md) | gauswavf | wavelet.shape | medium | small | ddf4218 |
| [findings/wavelet/fbspwavf.md](findings/wavelet/fbspwavf.md) | fbspwavf | wavelet.shape | low | small | ddf4218 |
| [findings/wavelet/mexihat.md](findings/wavelet/mexihat.md) | mexihat | wavelet.shape | low | small | ddf4218 |
| [findings/wavelet/meyeraux.md](findings/wavelet/meyeraux.md) | meyeraux | wavelet.shape | low | small | ddf4218 |
| [findings/wavelet/morlet.md](findings/wavelet/morlet.md) | morlet | wavelet.shape | low | small | ddf4218 |
| [findings/wavelet/shanwavf.md](findings/wavelet/shanwavf.md) | shanwavf | wavelet.shape | low | small | ddf4218 |
| [findings/stats/normrnd.md](findings/stats/normrnd.md) | normrnd | stats.dist | high | medium | 8e48677 |
| [findings/stats/chi2rnd.md](findings/stats/chi2rnd.md) | chi2rnd | stats.dist | medium | small | 8e48677 |
| [findings/stats/trnd.md](findings/stats/trnd.md) | trnd | stats.dist | medium | small | 8e48677 |
| [findings/wavelet/wfilters.md](findings/wavelet/wfilters.md) | wfilters | wavelet.filt | critical | medium | 1c2df89 |
| [findings/wavelet/coifwavf.md](findings/wavelet/coifwavf.md) | coifwavf | wavelet.filt | medium | small | 1c2df89 |
| [findings/wavelet/dbwavf.md](findings/wavelet/dbwavf.md) | dbwavf | wavelet.filt | medium | medium | 1c2df89 |
| [findings/wavelet/symwavf.md](findings/wavelet/symwavf.md) | symwavf | wavelet.filt | medium | medium | 1c2df89 |
| [findings/wavelet/orthfilt.md](findings/wavelet/orthfilt.md) | orthfilt | wavelet.filt | low | small | 1c2df89 |
| [findings/wavelet/qmf.md](findings/wavelet/qmf.md) | qmf | wavelet.filt | low | small | 1c2df89 |
| [findings/stats/betarnd.md](findings/stats/betarnd.md) | betarnd | stats.dist | medium | small | 301e5a5 |
| [findings/stats/exprnd.md](findings/stats/exprnd.md) | exprnd | stats.dist | medium | small | 301e5a5 |
| [findings/stats/gamrnd.md](findings/stats/gamrnd.md) | gamrnd | stats.dist | medium | small | 301e5a5 |
| [findings/signal/envelope.md](findings/signal/envelope.md) | envelope | signal.transforms | critical | medium | 9bce106 |
| [findings/stats/frnd.md](findings/stats/frnd.md) | frnd | stats.dist | medium | small | e580a5c |
| [findings/stats/raylrnd.md](findings/stats/raylrnd.md) | raylrnd | stats.dist | medium | small | e580a5c |
| [findings/stats/unifrnd.md](findings/stats/unifrnd.md) | unifrnd | stats.dist | medium | small | e580a5c |
| [findings/cluster/cluster.md](findings/cluster/cluster.md) | cluster | stats.cluster | medium | small | b2f133b |
| [findings/cluster/linkage.md](findings/cluster/linkage.md) | linkage | stats.cluster | medium | small | b2f133b |
| [findings/cluster/pdist.md](findings/cluster/pdist.md) | pdist | stats.cluster | medium | small | b2f133b |
| [findings/cluster/pdist2.md](findings/cluster/pdist2.md) | pdist2 | stats.cluster | medium | small | b2f133b |
| [findings/cluster/clusterdata.md](findings/cluster/clusterdata.md) | clusterdata | stats.cluster | low | small | b2f133b |
| [findings/cluster/cophenet.md](findings/cluster/cophenet.md) | cophenet | stats.cluster | low | small | b2f133b |
| [findings/cluster/dbscan.md](findings/cluster/dbscan.md) | dbscan | stats.cluster | low | small | b2f133b |
| [findings/cluster/inconsistent.md](findings/cluster/inconsistent.md) | inconsistent | stats.cluster | low | small | b2f133b |
| [findings/cluster/kmeans.md](findings/cluster/kmeans.md) | kmeans | stats.cluster | low | small | b2f133b |
| [findings/cluster/kmedoids.md](findings/cluster/kmedoids.md) | kmedoids | stats.cluster | low | small | b2f133b |
| [findings/cluster/mahal.md](findings/cluster/mahal.md) | mahal | stats.cluster | low | small | b2f133b |
| [findings/cluster/squareform.md](findings/cluster/squareform.md) | squareform | stats.cluster | low | small | b2f133b |
| [findings/stats/binornd.md](findings/stats/binornd.md) | binornd | stats.dist | low | small | 1525319 |
| [findings/stats/poissrnd.md](findings/stats/poissrnd.md) | poissrnd | stats.dist | low | small | 1525319 |
| [findings/stats/unidrnd.md](findings/stats/unidrnd.md) | unidrnd | stats.dist | low | small | 1525319 |
| [findings/regress/regress.md](findings/regress/regress.md) | regress | stats.regress | high | medium | f92087f |
| [findings/regress/lscov.md](findings/regress/lscov.md) | lscov | stats.regress | low | small | f92087f |
| [findings/regress/ridge.md](findings/regress/ridge.md) | ridge | stats.regress | low | small | f92087f |
| [findings/dim/pca.md](findings/dim/pca.md) | pca | stats.dim | low | small | f92087f |
| [findings/dim/pcacov.md](findings/dim/pcacov.md) | pcacov | stats.dim | low | small | f92087f |
| [findings/dim/pcares.md](findings/dim/pcares.md) | pcares | stats.dim | low | small | f92087f |
| [findings/empirical/ecdf.md](findings/empirical/ecdf.md) | ecdf | stats.empirical | low | small | f92087f |
| [findings/empirical/ecdfhist.md](findings/empirical/ecdfhist.md) | ecdfhist | stats.empirical | low | small | f92087f |
| [findings/empirical/ksdensity.md](findings/empirical/ksdensity.md) | ksdensity | stats.empirical | low | small | f92087f |
| [findings/lda/classify.md](findings/lda/classify.md) | classify | stats.lda | low | small | f92087f |
| [findings/mvdist/mnpdf.md](findings/mvdist/mnpdf.md) | mnpdf | stats.mvdist | low | small | f92087f |
| [findings/mvdist/mvnpdf.md](findings/mvdist/mvnpdf.md) | mvnpdf | stats.mvdist | low | small | f92087f |
| [findings/mvdist/mvtpdf.md](findings/mvdist/mvtpdf.md) | mvtpdf | stats.mvdist | low | small | f92087f |
| [findings/stats/lognrnd.md](findings/stats/lognrnd.md) | lognrnd | stats.dist | medium | small | 105c2b4 |
| [findings/stats/wblrnd.md](findings/stats/wblrnd.md) | wblrnd | stats.dist | medium | small | 105c2b4 |

## Closed ТЗ

| File | Function | Closed in commit | Closed date |
|---|---|---|---|
| [closed/stats/movmean.md](closed/stats/movmean.md) | movmean | PENDING | 2026-05-06 |
| [closed/stats/movmedian.md](closed/stats/movmedian.md) | movmedian | PENDING | 2026-05-06 |
| [closed/stats/movsum.md](closed/stats/movsum.md) | movsum | PENDING | 2026-05-06 |
| [closed/stats/movmin.md](closed/stats/movmin.md) | movmin | PENDING | 2026-05-06 |
| [closed/stats/movmax.md](closed/stats/movmax.md) | movmax | PENDING | 2026-05-06 |
| [closed/stats/movprod.md](closed/stats/movprod.md) | movprod | PENDING | 2026-05-06 |
| [closed/stats/movmad.md](closed/stats/movmad.md) | movmad | PENDING | 2026-05-06 |
| [closed/stats/movstd.md](closed/stats/movstd.md) | movstd | PENDING | 2026-05-06 |
| [closed/stats/movvar.md](closed/stats/movvar.md) | movvar | PENDING | 2026-05-06 |
| [closed/stats/iqr.md](closed/stats/iqr.md) | iqr | PENDING | 2026-05-06 |
| [closed/stats/quantile.md](closed/stats/quantile.md) | quantile | PENDING | 2026-05-06 |
| [closed/stats/prctile.md](closed/stats/prctile.md) | prctile | PENDING | 2026-05-06 |
| [closed/stats/var.md](closed/stats/var.md) | var | PENDING | 2026-05-06 |
| [closed/stats/std.md](closed/stats/std.md) | std | PENDING | 2026-05-06 |
| [closed/stats/normcdf.md](closed/stats/normcdf.md) | normcdf | PENDING | 2026-05-06 |
| [closed/stats/chi2cdf.md](closed/stats/chi2cdf.md) | chi2cdf | PENDING | 2026-05-06 |
| [closed/stats/tcdf.md](closed/stats/tcdf.md) | tcdf | PENDING | 2026-05-06 |
| [closed/stats/fcdf.md](closed/stats/fcdf.md) | fcdf | PENDING | 2026-05-06 |
| [closed/stats/betacdf.md](closed/stats/betacdf.md) | betacdf | PENDING | 2026-05-06 |
| [closed/stats/gamcdf.md](closed/stats/gamcdf.md) | gamcdf | PENDING | 2026-05-06 |
| [closed/stats/expcdf.md](closed/stats/expcdf.md) | expcdf | PENDING | 2026-05-06 |
| [closed/stats/raylcdf.md](closed/stats/raylcdf.md) | raylcdf | PENDING | 2026-05-06 |
| [closed/stats/logncdf.md](closed/stats/logncdf.md) | logncdf | PENDING | 2026-05-06 |
| [closed/stats/wblcdf.md](closed/stats/wblcdf.md) | wblcdf | PENDING | 2026-05-06 |
| [closed/stats/unifcdf.md](closed/stats/unifcdf.md) | unifcdf | PENDING | 2026-05-06 |
| [closed/stats/unidcdf.md](closed/stats/unidcdf.md) | unidcdf | PENDING | 2026-05-06 |
| [closed/stats/binocdf.md](closed/stats/binocdf.md) | binocdf | PENDING | 2026-05-06 |
| [closed/stats/poisscdf.md](closed/stats/poisscdf.md) | poisscdf | PENDING | 2026-05-06 |
| [closed/signal/hamming.md](closed/signal/hamming.md) | hamming | PENDING | 2026-05-06 |
| [closed/signal/hann.md](closed/signal/hann.md) | hann | PENDING | 2026-05-06 |
| [closed/signal/blackman.md](closed/signal/blackman.md) | blackman | PENDING | 2026-05-06 |
| [closed/signal/blackmanharris.md](closed/signal/blackmanharris.md) | blackmanharris | PENDING | 2026-05-06 |
| [closed/signal/flattopwin.md](closed/signal/flattopwin.md) | flattopwin | PENDING | 2026-05-06 |
| [closed/signal/nuttallwin.md](closed/signal/nuttallwin.md) | nuttallwin | PENDING | 2026-05-06 |
| [closed/signal/bartlett.md](closed/signal/bartlett.md) | bartlett | PENDING | 2026-05-06 |
| [closed/signal/triang.md](closed/signal/triang.md) | triang | PENDING | 2026-05-06 |
| [closed/signal/parzenwin.md](closed/signal/parzenwin.md) | parzenwin | PENDING | 2026-05-06 |
| [closed/signal/bohmanwin.md](closed/signal/bohmanwin.md) | bohmanwin | PENDING | 2026-05-06 |
| [closed/signal/barthannwin.md](closed/signal/barthannwin.md) | barthannwin | PENDING | 2026-05-06 |
| [closed/signal/rectwin.md](closed/signal/rectwin.md) | rectwin | PENDING | 2026-05-06 |
| [closed/signal/hilbert.md](closed/signal/hilbert.md) | hilbert | PENDING | 2026-05-06 |
| [closed/stats/kstest.md](closed/stats/kstest.md) | kstest | PENDING | 2026-05-06 |
| [closed/stats/kstest2.md](closed/stats/kstest2.md) | kstest2 | PENDING | 2026-05-06 |
| [closed/stats/ttest.md](closed/stats/ttest.md) | ttest | PENDING (partial) | 2026-05-06 |
| [closed/stats/ttest2.md](closed/stats/ttest2.md) | ttest2 | PENDING (partial) | 2026-05-06 |
| [closed/stats/vartest.md](closed/stats/vartest.md) | vartest | PENDING (partial) | 2026-05-06 |
| [closed/stats/vartest2.md](closed/stats/vartest2.md) | vartest2 | PENDING (partial) | 2026-05-06 |
| [closed/stats/ztest.md](closed/stats/ztest.md) | ztest | PENDING (partial) | 2026-05-06 |
| [closed/stats/signtest.md](closed/stats/signtest.md) | signtest | PENDING | 2026-05-06 |
| [closed/stats/signrank.md](closed/stats/signrank.md) | signrank | PENDING | 2026-05-06 |
| [closed/stats/ranksum.md](closed/stats/ranksum.md) | ranksum | PENDING | 2026-05-06 |
| [closed/stats/fishertest.md](closed/stats/fishertest.md) | fishertest | PENDING | 2026-05-06 |
| [closed/stats/bounds.md](closed/stats/bounds.md) | bounds | PENDING (partial) | 2026-05-06 |
| [closed/stats/mode.md](closed/stats/mode.md) | mode | PENDING (partial) | 2026-05-06 |
| [closed/stats/cummax.md](closed/stats/cummax.md) | cummax | PENDING | 2026-05-06 |
| [closed/stats/cummin.md](closed/stats/cummin.md) | cummin | PENDING | 2026-05-06 |
| [closed/stats/rmse.md](closed/stats/rmse.md) | rmse | PENDING (partial) | 2026-05-06 |
| [closed/stats/mape.md](closed/stats/mape.md) | mape | PENDING (partial) | 2026-05-06 |
| [closed/stats/maxk.md](closed/stats/maxk.md) | maxk | PENDING (partial) | 2026-05-06 |
| [closed/stats/mink.md](closed/stats/mink.md) | mink | PENDING (partial) | 2026-05-06 |
| [closed/stats/runstest.md](closed/stats/runstest.md) | runstest | PENDING (partial) | 2026-05-06 |
| [closed/stats/median.md](closed/stats/median.md) | median | PENDING (partial) | 2026-05-06 |
| [closed/stats/kruskalwallis.md](closed/stats/kruskalwallis.md) | kruskalwallis | PENDING | 2026-05-06 |
| [closed/stats/binofit.md](closed/stats/binofit.md) | binofit | PENDING | 2026-05-06 |
| [closed/stats/normlike.md](closed/stats/normlike.md) | normlike | PENDING | 2026-05-06 |
| [closed/stats/gamlike.md](closed/stats/gamlike.md) | gamlike | PENDING | 2026-05-06 |
| [closed/stats/betalike.md](closed/stats/betalike.md) | betalike | PENDING | 2026-05-06 |
| [closed/stats/gevlike.md](closed/stats/gevlike.md) | gevlike | PENDING (partial) | 2026-05-06 |
| [closed/stats/gplike.md](closed/stats/gplike.md) | gplike | PENDING | 2026-05-06 |
| [closed/stats/explike.md](closed/stats/explike.md) | explike | PENDING | 2026-05-06 |
| [closed/stats/lognlike.md](closed/stats/lognlike.md) | lognlike | PENDING | 2026-05-06 |
| [closed/stats/betapdf.md](closed/stats/betapdf.md) | betapdf | PENDING | 2026-05-07 |
| [closed/stats/betainv.md](closed/stats/betainv.md) | betainv | PENDING | 2026-05-07 |
| [closed/stats/betastat.md](closed/stats/betastat.md) | betastat | PENDING | 2026-05-07 |
| [closed/stats/chi2pdf.md](closed/stats/chi2pdf.md) | chi2pdf | PENDING | 2026-05-07 |
| [closed/stats/chi2inv.md](closed/stats/chi2inv.md) | chi2inv | PENDING | 2026-05-07 |
| [closed/stats/chi2stat.md](closed/stats/chi2stat.md) | chi2stat | PENDING | 2026-05-07 |
| [closed/stats/expinv.md](closed/stats/expinv.md) | expinv | PENDING | 2026-05-07 |
| [closed/stats/expstat.md](closed/stats/expstat.md) | expstat | PENDING | 2026-05-07 |
| [closed/stats/finv.md](closed/stats/finv.md) | finv | PENDING | 2026-05-07 |
| [closed/stats/fpdf.md](closed/stats/fpdf.md) | fpdf | PENDING | 2026-05-07 |
| [closed/stats/fstat.md](closed/stats/fstat.md) | fstat | PENDING | 2026-05-07 |
| [closed/stats/norminv.md](closed/stats/norminv.md) | norminv | PENDING | 2026-05-07 |
| [closed/stats/normpdf.md](closed/stats/normpdf.md) | normpdf | PENDING | 2026-05-07 |
| [closed/stats/normstat.md](closed/stats/normstat.md) | normstat | PENDING | 2026-05-07 |
| [closed/stats/poisstat.md](closed/stats/poisstat.md) | poisstat | PENDING | 2026-05-07 |
| [closed/stats/raylstat.md](closed/stats/raylstat.md) | raylstat | PENDING | 2026-05-07 |
| [closed/stats/tstat.md](closed/stats/tstat.md) | tstat | PENDING | 2026-05-07 |
| [closed/stats/unidstat.md](closed/stats/unidstat.md) | unidstat | PENDING | 2026-05-07 |
| [closed/stats/lognstat.md](closed/stats/lognstat.md) | lognstat | PENDING | 2026-05-07 |
| [closed/stats/gamstat.md](closed/stats/gamstat.md) | gamstat | PENDING | 2026-05-07 |
| [closed/stats/unifstat.md](closed/stats/unifstat.md) | unifstat | PENDING | 2026-05-07 |
| [closed/stats/wblstat.md](closed/stats/wblstat.md) | wblstat | PENDING | 2026-05-07 |
| [closed/stats/binostat.md](closed/stats/binostat.md) | binostat | PENDING | 2026-05-07 |
| [closed/stats/binoinv.md](closed/stats/binoinv.md) | binoinv | PENDING | 2026-05-07 |
| [closed/stats/binopdf.md](closed/stats/binopdf.md) | binopdf | PENDING | 2026-05-07 |
| [closed/stats/exppdf.md](closed/stats/exppdf.md) | exppdf | PENDING | 2026-05-07 |
| [closed/stats/gampdf.md](closed/stats/gampdf.md) | gampdf | PENDING | 2026-05-07 |
| [closed/stats/gaminv.md](closed/stats/gaminv.md) | gaminv | PENDING | 2026-05-07 |
| [closed/stats/lognpdf.md](closed/stats/lognpdf.md) | lognpdf | PENDING | 2026-05-07 |
| [closed/stats/logninv.md](closed/stats/logninv.md) | logninv | PENDING | 2026-05-07 |
| [closed/stats/poisspdf.md](closed/stats/poisspdf.md) | poisspdf | PENDING | 2026-05-07 |
| [closed/stats/poissinv.md](closed/stats/poissinv.md) | poissinv | PENDING | 2026-05-07 |
| [closed/stats/raylpdf.md](closed/stats/raylpdf.md) | raylpdf | PENDING | 2026-05-07 |
| [closed/stats/raylinv.md](closed/stats/raylinv.md) | raylinv | PENDING | 2026-05-08 |
| [closed/stats/tinv.md](closed/stats/tinv.md) | tinv | PENDING | 2026-05-08 |
| [closed/stats/tpdf.md](closed/stats/tpdf.md) | tpdf | PENDING | 2026-05-08 |
| [closed/stats/unidpdf.md](closed/stats/unidpdf.md) | unidpdf | PENDING | 2026-05-08 |
| [closed/stats/unidinv.md](closed/stats/unidinv.md) | unidinv | PENDING | 2026-05-08 |
| [closed/stats/unifpdf.md](closed/stats/unifpdf.md) | unifpdf | PENDING | 2026-05-08 |
| [closed/stats/unifinv.md](closed/stats/unifinv.md) | unifinv | PENDING | 2026-05-08 |
| [closed/stats/wblpdf.md](closed/stats/wblpdf.md) | wblpdf | PENDING | 2026-05-08 |
| [closed/stats/wblinv.md](closed/stats/wblinv.md) | wblinv | PENDING | 2026-05-08 |
| [closed/stats/expfit.md](closed/stats/expfit.md) | expfit | PENDING | 2026-05-08 |
| [closed/stats/poissfit.md](closed/stats/poissfit.md) | poissfit | PENDING | 2026-05-08 |
| [closed/stats/unifit.md](closed/stats/unifit.md) | unifit | PENDING | 2026-05-08 |
| [closed/stats/raylfit.md](closed/stats/raylfit.md) | raylfit | PENDING | 2026-05-08 |
| [closed/stats/evlike.md](closed/stats/evlike.md) | evlike | PENDING (partial) | 2026-05-08 |
| [closed/stats/wbllike.md](closed/stats/wbllike.md) | wbllike | PENDING (partial) | 2026-05-08 |
| [closed/signal/fftshift.md](closed/signal/fftshift.md) | fftshift | PENDING | 2026-05-08 |
| [closed/signal/ifftshift.md](closed/signal/ifftshift.md) | ifftshift | PENDING | 2026-05-08 |
| [closed/signal/nextpow2.md](closed/signal/nextpow2.md) | nextpow2 | PENDING | 2026-05-08 |
| [closed/signal/dftmtx.md](closed/signal/dftmtx.md) | dftmtx | PENDING | 2026-05-08 |
| [closed/signal/interpft.md](closed/signal/interpft.md) | interpft | PENDING | 2026-05-08 |
| [closed/signal/bitrevorder.md](closed/signal/bitrevorder.md) | bitrevorder | PENDING | 2026-05-08 |
| [closed/signal/dct.md](closed/signal/dct.md) | dct | PENDING (partial) | 2026-05-08 |
| [closed/signal/idct.md](closed/signal/idct.md) | idct | PENDING (partial) | 2026-05-08 |
| [closed/signal/enbw.md](closed/signal/enbw.md) | enbw | PENDING | 2026-05-08 |
| [closed/signal/kaiser.md](closed/signal/kaiser.md) | kaiser | PENDING | 2026-05-08 |
| [closed/signal/gausswin.md](closed/signal/gausswin.md) | gausswin | PENDING | 2026-05-08 |
| [closed/signal/tukeywin.md](closed/signal/tukeywin.md) | tukeywin | PENDING | 2026-05-08 |
| [closed/signal/chebwin.md](closed/signal/chebwin.md) | chebwin | PENDING | 2026-05-08 |
| [closed/signal/taylorwin.md](closed/signal/taylorwin.md) | taylorwin | PENDING | 2026-05-08 |
| [closed/wavelet/wrev.md](closed/wavelet/wrev.md) | wrev | PENDING | 2026-05-08 |

---

## How this file is updated

- **Auditor** appends new rows to "Open ТЗ" when creating ТЗ.
- **Main worker** moves rows from "Open ТЗ" to "Closed ТЗ" after fix
  lands and updates the file path to point at `audit/closed/...`.
- Neither worker rewrites historical rows once closed.

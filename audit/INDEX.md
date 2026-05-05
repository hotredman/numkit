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
| [findings/stats/betalike.md](findings/stats/betalike.md) | betalike | stats.fit | medium | small | bfda361 |
| [findings/stats/evlike.md](findings/stats/evlike.md) | evlike | stats.fit | medium | medium | bfda361 |
| [findings/stats/expfit.md](findings/stats/expfit.md) | expfit | stats.fit | medium | medium | bfda361 |
| [findings/stats/explike.md](findings/stats/explike.md) | explike | stats.fit | medium | medium | bfda361 |
| [findings/stats/gamlike.md](findings/stats/gamlike.md) | gamlike | stats.fit | medium | small | bfda361 |
| [findings/stats/gevlike.md](findings/stats/gevlike.md) | gevlike | stats.fit | medium | small | bfda361 |
| [findings/stats/gplike.md](findings/stats/gplike.md) | gplike | stats.fit | medium | small | bfda361 |
| [findings/stats/lognfit.md](findings/stats/lognfit.md) | lognfit | stats.fit | medium | medium | bfda361 |
| [findings/stats/lognlike.md](findings/stats/lognlike.md) | lognlike | stats.fit | medium | medium | bfda361 |
| [findings/stats/normfit.md](findings/stats/normfit.md) | normfit | stats.fit | medium | medium | bfda361 |
| [findings/stats/normlike.md](findings/stats/normlike.md) | normlike | stats.fit | medium | medium | bfda361 |
| [findings/stats/wbllike.md](findings/stats/wbllike.md) | wbllike | stats.fit | medium | medium | bfda361 |
| [findings/stats/binofit.md](findings/stats/binofit.md) | binofit | stats.fit | low | small | bfda361 |
| [findings/stats/poissfit.md](findings/stats/poissfit.md) | poissfit | stats.fit | low | small | bfda361 |
| [findings/stats/raylfit.md](findings/stats/raylfit.md) | raylfit | stats.fit | low | small | bfda361 |
| [findings/stats/unifit.md](findings/stats/unifit.md) | unifit | stats.fit | low | small | bfda361 |
| [findings/stats/chi2gof.md](findings/stats/chi2gof.md) | chi2gof | stats.test | medium | large | 69fab7c |
| [findings/stats/jbtest.md](findings/stats/jbtest.md) | jbtest | stats.test | medium | medium | 69fab7c |
| [findings/stats/kruskalwallis.md](findings/stats/kruskalwallis.md) | kruskalwallis | stats.test | medium | medium | 69fab7c |
| [findings/stats/kstest.md](findings/stats/kstest.md) | kstest | stats.test | high | medium | 69fab7c |
| [findings/stats/kstest2.md](findings/stats/kstest2.md) | kstest2 | stats.test | high | small | 69fab7c |
| [findings/stats/runstest.md](findings/stats/runstest.md) | runstest | stats.test | high | medium | 69fab7c |
| [findings/stats/ttest.md](findings/stats/ttest.md) | ttest | stats.test | high | medium | 69fab7c |
| [findings/stats/ttest2.md](findings/stats/ttest2.md) | ttest2 | stats.test | high | medium | 69fab7c |
| [findings/stats/vartest.md](findings/stats/vartest.md) | vartest | stats.test | medium | small | 69fab7c |
| [findings/stats/vartest2.md](findings/stats/vartest2.md) | vartest2 | stats.test | medium | small | 69fab7c |
| [findings/stats/vartestn.md](findings/stats/vartestn.md) | vartestn | stats.test | medium | medium | 69fab7c |
| [findings/stats/ztest.md](findings/stats/ztest.md) | ztest | stats.test | medium | small | 69fab7c |
| [findings/stats/fishertest.md](findings/stats/fishertest.md) | fishertest | stats.test | low | small | 69fab7c |
| [findings/stats/ranksum.md](findings/stats/ranksum.md) | ranksum | stats.test | low | small | 69fab7c |
| [findings/stats/signrank.md](findings/stats/signrank.md) | signrank | stats.test | low | small | 69fab7c |
| [findings/stats/signtest.md](findings/stats/signtest.md) | signtest | stats.test | low | small | 69fab7c |
| [findings/stats/movmad.md](findings/stats/movmad.md) | movmad | stats.moving | critical | small | 4f021db |
| [findings/stats/movmax.md](findings/stats/movmax.md) | movmax | stats.moving | critical | small | 4f021db |
| [findings/stats/movmean.md](findings/stats/movmean.md) | movmean | stats.moving | critical | large | 4f021db |
| [findings/stats/movmedian.md](findings/stats/movmedian.md) | movmedian | stats.moving | critical | small | 4f021db |
| [findings/stats/movmin.md](findings/stats/movmin.md) | movmin | stats.moving | critical | small | 4f021db |
| [findings/stats/movprod.md](findings/stats/movprod.md) | movprod | stats.moving | critical | small | 4f021db |
| [findings/stats/movstd.md](findings/stats/movstd.md) | movstd | stats.moving | critical | small | 4f021db |
| [findings/stats/movsum.md](findings/stats/movsum.md) | movsum | stats.moving | critical | small | 4f021db |
| [findings/stats/movvar.md](findings/stats/movvar.md) | movvar | stats.moving | critical | small | 4f021db |
| [findings/stats/cummax.md](findings/stats/cummax.md) | cummax | builtin | high | medium | 4f021db |
| [findings/stats/cummin.md](findings/stats/cummin.md) | cummin | builtin | high | medium | 4f021db |
| [findings/stats/iqr.md](findings/stats/iqr.md) | iqr | stats.descriptive | high | small | ba142e6 |
| [findings/stats/median.md](findings/stats/median.md) | median | stats.descriptive | high | medium | ba142e6 |
| [findings/stats/prctile.md](findings/stats/prctile.md) | prctile | stats.descriptive | high | small | ba142e6 |
| [findings/stats/quantile.md](findings/stats/quantile.md) | quantile | stats.descriptive | high | medium | ba142e6 |
| [findings/stats/std.md](findings/stats/std.md) | std | stats.descriptive | high | small | ba142e6 |
| [findings/stats/var.md](findings/stats/var.md) | var | stats.descriptive | high | medium | ba142e6 |
| [findings/stats/bounds.md](findings/stats/bounds.md) | bounds | stats.descriptive | medium | small | ba142e6 |
| [findings/stats/mape.md](findings/stats/mape.md) | mape | stats.descriptive | medium | small | ba142e6 |
| [findings/stats/maxk.md](findings/stats/maxk.md) | maxk | stats.descriptive | medium | small | ba142e6 |
| [findings/stats/mink.md](findings/stats/mink.md) | mink | stats.descriptive | medium | small | ba142e6 |
| [findings/stats/mode.md](findings/stats/mode.md) | mode | stats.descriptive | medium | small | ba142e6 |
| [findings/stats/rmse.md](findings/stats/rmse.md) | rmse | stats.descriptive | medium | small | ba142e6 |
| [findings/signal/blackman.md](findings/signal/blackman.md) | blackman | signal.windows | high | small | 0e043c5 |
| [findings/signal/blackmanharris.md](findings/signal/blackmanharris.md) | blackmanharris | signal.windows | high | small | 0e043c5 |
| [findings/signal/flattopwin.md](findings/signal/flattopwin.md) | flattopwin | signal.windows | high | small | 0e043c5 |
| [findings/signal/hamming.md](findings/signal/hamming.md) | hamming | signal.windows | high | small | 0e043c5 |
| [findings/signal/hann.md](findings/signal/hann.md) | hann | signal.windows | high | small | 0e043c5 |
| [findings/signal/nuttallwin.md](findings/signal/nuttallwin.md) | nuttallwin | signal.windows | high | small | 0e043c5 |
| [findings/signal/barthannwin.md](findings/signal/barthannwin.md) | barthannwin | signal.windows | low | small | 0e043c5 |
| [findings/signal/bartlett.md](findings/signal/bartlett.md) | bartlett | signal.windows | low | small | 0e043c5 |
| [findings/signal/bohmanwin.md](findings/signal/bohmanwin.md) | bohmanwin | signal.windows | low | small | 0e043c5 |
| [findings/signal/parzenwin.md](findings/signal/parzenwin.md) | parzenwin | signal.windows | low | small | 0e043c5 |
| [findings/signal/rectwin.md](findings/signal/rectwin.md) | rectwin | signal.windows | low | small | 0e043c5 |
| [findings/signal/triang.md](findings/signal/triang.md) | triang | signal.windows | low | small | 0e043c5 |
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
| [findings/signal/chebwin.md](findings/signal/chebwin.md) | chebwin | signal.windows | critical | medium | 69ef496 |
| [findings/signal/taylorwin.md](findings/signal/taylorwin.md) | taylorwin | signal.windows | critical | medium | 69ef496 |
| [findings/signal/enbw.md](findings/signal/enbw.md) | enbw | signal.windows | low | small | 69ef496 |
| [findings/signal/gausswin.md](findings/signal/gausswin.md) | gausswin | signal.windows | low | small | 69ef496 |
| [findings/signal/kaiser.md](findings/signal/kaiser.md) | kaiser | signal.windows | low | small | 69ef496 |
| [findings/signal/tukeywin.md](findings/signal/tukeywin.md) | tukeywin | signal.windows | low | small | 69ef496 |

## Closed ТЗ

| File | Function | Closed in commit | Closed date |
|---|---|---|---|
| _none yet_ | | | |

---

## How this file is updated

- **Auditor** appends new rows to "Open ТЗ" when creating ТЗ.
- **Main worker** moves rows from "Open ТЗ" to "Closed ТЗ" after fix
  lands and updates the file path to point at `audit/closed/...`.
- Neither worker rewrites historical rows once closed.

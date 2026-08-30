# Parity-agent prompt — autonomous MATLAB-parity cycle

> **Status: reference runbook — NOT currently scheduled.** This is the cold-start
> prompt that drove the autonomous parity-bench cron agent. The cycle is not
> running right now; it is kept here as the canonical procedure for if/when it is
> re-armed. Re-check the concrete facts (paths, test count, branch protocol)
> before re-arming — they drift.

Ты — scheduled-агент Claude Code, продолжающий автономный MATLAB-парити-цикл для
проекта numkit. Стартуешь cold; вся история — в memory + git.

## Сначала прочти в этом порядке

1. `AGENTS.md` — project rules (стоп-и-сверься, если `git status` грязный).
2. `C:/Users/User/.claude/projects/C--Users-User-Projects-megahard-numkit/memory/MEMORY.md`
3. `…/memory/project_parity_cycle_progress.md` ← **главный документ: что сделано,
   текущая цель, правила.**
4. `bugs/README.md` — индекс структурного баг-каталога (known issues — **НЕ**
   считать их регрессиями).
5. `tools/parity/PROGRESS.md` — живая карта парити (ищешь строки с пустыми
   ячейками).

## Текущая цель

Honest per-signature bulk-bench. **НЕ** реализовывать новые функции. Для каждой
✅ функции из `PROGRESS.md` протестировать **каждую документированную сигнатуру**
и записать честный результат в `{OK | MISMATCH | THROW | MISSING | SKIP}`.

## Один раунд работы (этот scheduled-запуск)

### A. Подготовка

1. На рабочей ветке `lib-dev`: `git fetch origin && git rebase origin/main`
   (избежать конфликта с ручными коммитами пользователя).
2. `git status` — должно быть чисто. Если нет — **НЕ** продолжать, записать в
   diary `STUCK: dirty tree` и выйти без коммита.
3. Прочитать `bugs/README.md` и `project_parity_cycle_progress.md` полностью.

### B. Выбор работы

4. Из `PROGRESS.md` выбери **до 30 функций** со статусом ✅ и пустой ячейкой
   `numkit_ms` (приоритет: верхние секции — Trigonometry / Arithmetic / Special
   Functions). Пропусти секции с namespace `categorical.*`, `table.*`,
   `datetime.*`, `ode.*`, `wavelet.*`, `linalg.*` — они требуют отсутствующих
   типов и помечаются `SKIP`.
5. Для каждой собери все сигнатуры через MATLAB `help <fn>` (`matlab -batch`).
   Логи разбора — в `tools/parity/sigs/<fn>.json`.

### C. Auto-spec

6. Для каждой сигнатуры сгенерируй spec в
   `tools/parity/specs_auto/<fn>__<sig_id>.json` по эвристике:
   - unary numeric: `setup x=linspace(...)`; `expr y=fn(x)`
   - binary numeric: `x, y`; `z=fn(x,y)`
   - reduction: `s=fn(x)`
   - constructor: `A=fn(N, M)`
   - predicate: `p=fn(x)`

   Auto-спеки, которые точно не сработают (string-cell, multi-output
   destructure, side-effects типа figure/disp/error), — пометь
   `SKIP: needs manual spec` в comment-ячейке `PROGRESS`, не запускай.

### D. Прогон

7. `python tools/parity/run_parity.py specs_auto/*.json` (или итеративно
   поспецово). Harness обновляет `PROGRESS.md` на месте; если функция в
   нескольких секциях — все вхождения.
8. Любое расхождение (`MISMATCH` / `THROW`) → **заведи запись в баг-каталоге**
   (плоского `BUGS.md` больше нет — он расформирован в `bugs/`):
   - `bugs/<namespace>/<fn>.md` — самодостаточный repro (numkit output vs MATLAB
     R2025b), тег `Kind:` (`bug` / `stub` / `missing-output` / `missing-fn` /
     `perf`), expected (MATLAB), actual (numkit), root cause (toolboxes/ имя файла —
     или `core, area unknown`), `Status: 🔴 OPEN`;
   - матчащий **`DISABLED_` gtest** в `toolboxes/<ns>/tests/known_bugs_test.cpp`,
     утверждающий MATLAB-корректное поведение (found a bug → add a test);
   - строка в индексе `bugs/README.md` (+ обнови tally).

   Шаблон + легенда `Kind` — в `bugs/README.md`.

### E. Валидация

9. Полный test suite (с отфильтрованными известными flake'ами — актуальный
   список см. `memory/feedback_pre_existing_eval_test_flake.md`):
   ```
   build/desktop-fast/tests/gtest/Release/numkit_gtest.exe \
     --gtest_filter="-TW_VM/EvalRegressionTest.AssignmentCaptureSuppressesInnerAns/*:\
   TW_VM/EvalRegressionTest.BracketConcatInLoopInsideFunction/*" \
     --gtest_brief=1
   ```
   Должно быть **11611+ passed**. Если меньше — в diary `STUCK: regression` и
   выйти.

### F. Commit + push

10. Коммит на `lib-dev`:
    ```
    parity bulk-bench: <N fns, M sigs>: A OK, B MISMATCH, C THROW, D MISSING, E SKIP

    <бельевая роспись по секциям и заметные расхождения>
    New bugs/ entries: <fn-list>

    Co-Authored-By: Claude Opus 4.8 (1M context) <noreply@anthropic.com>
    ```
11. `git push origin lib-dev`, затем re-fetch `origin/main` + ff-push
    `lib-dev:main` (merge сначала, если main сдвинулся; **никогда** не
    force-push `main`). Если `bench_*` перетёр `BENCHMARK.md` — восстановить.

## Правила (НЕ нарушать)

- **toolboxes/ only.** `core/` не трогать. Каждый core-баг → запись в `bugs/`
  каталог (`Kind: bug`, root cause `core`), **не** фиксить без явного
  разрешения пользователя.
- Если spec не работает с auto-параметрами — **НЕ** закрывать «OK по
  ассоциации». Честно пометить `MISSING` / `auto-spec: signature unknown`.
- Commit body на английском (AGENTS.md repo norm). Conversational — можно в
  комментариях задач, не в commit.
- Не коммитить, если test suite упал. Не коммитить пустых изменений.

## Условия остановки

Останавливайся (не коммитить + push, отметить только в diary), если:

- `git status` грязный после rebase (конфликт с ручной работой пользователя);
- test suite потерял ранее проходившие тесты (регрессия из-за чужих параллельных
  коммитов);
- MATLAB или Octave не запускается (пути — в
  `memory/reference_matlab_octave_paths.md`);
- контекст близок к лимиту — лучше commit + push текущего прогресса и
  завершиться; следующий запуск продолжит с актуального `PROGRESS.md`.

Завершайся успешно, если:

- все 30 функций раунда обработаны;
- `PROGRESS.md` показывает: 100% строк ✅ имеют либо `numkit_ms`-замер, либо явный
  `SKIP`-comment. Если так — отметь в commit `SWEEP COMPLETE`, следующие запуски
  будут no-op.

## Полезные контексты

- **Harness:** `tools/parity/run_parity.py` — `--all` итерирует все спеки в
  `tools/parity/specs/` и `tools/parity/specs_auto/`.
- **Spec-формат:** ~1500 примеров в `tools/parity/specs/` (по одному на
  функцию/ветку).
- **MATLAB:** `matlab -batch "<expr>"` (R2025b на PATH; ~3 s startup).
- **Octave:** `"/c/Program Files/GNU Octave/Octave-11.1.0/mingw64/bin/octave-cli.exe"
  --no-gui /tmp/script.m` (**не** через `--eval` со сложными `"`-литералами —
  Windows argv их съедает; пиши temp `.m`).
- MATLAB-переменные не могут начинаться с `_` — используй `sv__`/`flat__`/`i__`/
  `el__` вместо `__sv` и т.п.
- `/c/Program Files/MATLAB/R2025b/bin/matlab` — fallback-путь.

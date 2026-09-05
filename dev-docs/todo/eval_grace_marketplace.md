# todo: evaluate GRACE marketplace IF the multi-agent model is revived

*Kind:* deferred-decision · *Status:* closed-not-adopted (2026-09-01) ·
*Surfaced:* 2026-09-01

**Decision.** GRACE (osovv/grace-marketplace, contract-driven AI
engineering: .grace/ XML artifacts, semantic source markup, spec→plan→
execute skills) will NOT be adopted. User decision after review.

**Why not** (agreed reasoning):
- numkit's protocol is already mechanically enforced where GRACE is
  declarative: bugs_audit.py links bugs↔guards, parity specs diff
  against the live MATLAB R2025b reference, compare groups produce
  PASS/KNOWN/NEW on the real corpus. GRACE would describe what we
  already execute.
- Dual governance (AGENTS.md + dev-docs vs .grace/changes + GRACE
  skills) is a net cost with no clear authority.
- Markup retrofit over a 4000-commit C++ codebase is disproportionate.
- Single-author, young methodology (Feb 2026) — too early to bet the
  core process on it.

**Revisit only if** the multi-agent parallel-session model is revived
(coordination.md): GRACE's change-scoping/drift-detection target that
specific pain. If so — a bounded pilot on ONE subsystem (e.g.
packages/numkit-mcp), not a repo-wide adoption.

This file exists so the question does not get re-litigated from scratch.

# numkit Clean-Room Reimplementation Protocol

## Purpose

The 2026-05 IP-provenance audit found that several functions in numkit's
libraries are structural translations of MATLAB's proprietary toolbox
`.m` source files (the "B list"). This protocol governs their
replacement with independent, clean-room implementations, so the
resulting code is numkit's own copyrightable expression.

**This protocol should be reviewed by an IP attorney before the
remediated code is relied upon for commercial licensing.**

## Scope

The B-list functions identified by the audit (tracked in project
memory `project_ip_provenance_remediation.md`). The protocol is applied
once per function.

## Roles

Two separated roles. For any given function, the same person/agent MUST
NOT hold both roles.

### Spec Author
- MAY have seen the tainted implementation and the audit reports.
- Produces a written Specification (`cleanroom/specs/<fn>.md`).
- The Specification is derived ONLY from public references and the
  numkit interface contract. It MUST NOT contain, paraphrase, or
  reconstruct:
  - MATLAB `.m` source code, its variable names, or its control-flow
    structure;
  - the tainted numkit implementation's structure;
  - expression copied from any third-party reference implementation.

### Implementer
- MUST NOT have seen, during the work: the MATLAB `.m` source, the
  tainted numkit implementation, the audit reports, or any third-party
  reference implementation under its own licence.
- Implements the function using ONLY the Specification, the cited
  public references, and numkit's own public API.
- MUST NOT open the tainted source file's affected function.

## Allowed / forbidden sources

Allowed: peer-reviewed papers, textbooks, public standards; numkit's
own headers and code (except the tainted function); MATLAB's **public
documentation** for documented default parameter values (those are
facts, not protected expression).

Forbidden: MATLAB `.m` source files; reference implementations under
their own licences (e.g. COVAREP).

## Specification contents

1. **Algorithm** — the mathematical method, step by step, cited to the
   public reference(s).
2. **Interface** — the exact numkit function signature, parameter and
   return types, and the numkit helper APIs to use.
3. **Compatibility parameters** — default values needed to match
   MATLAB behaviour, each sourced from MATLAB's public documentation
   or from black-box output probing (never from `.m` source).
4. **Verification criteria** — which parity specs / gtests apply and
   the acceptable tolerance.

## Verification

The new implementation is checked against the parity harness and the
gtests. Bit-exact parity with MATLAB on **undocumented** internal
details is NOT required and is not a goal — bit-exactness was the
hallmark of the original derivation. A reasonable numerical tolerance
is acceptable; any change from a previous `tol=0` / `1e-12` baseline is
recorded below.

## Per-function record

| Function | Public reference | Spec | Implementer | Impl commit | Verification |
|---|---|---|---|---|---|
| pitchCEP | Noll 1967 (JASA 41(2)) | cleanroom/specs/pitchCEP.md | clean-room agent (per spec) | (this commit) | gtest 16/16; parity correctness=OK |
| pitchLHS | Hermes 1988 (JASA 83(1)) | cleanroom/specs/pitchLHS.md | clean-room agent (per spec) | (this commit) | gtest 16/16; parity correctness=OK |
| pitchSRH | Drugman & Alwan 2011 (Interspeech) | (pending) | (pending) | (pending) | (pending) |
| pitchPEF | Gonzalez & Brookes 2011 (EUSIPCO) | (pending) | (pending) | (pending) | (pending) |
| designMelFilterBankSlaney | Slaney 1998 (Apple TR #45) | (pending) | (pending) | (pending) | (pending) |
| adapthisteq | Zuiderveld 1994 (Graphics Gems IV) | (pending) | (pending) | (pending) | (pending) |
| bwmorph | Lam/Lee/Suen 1992; Pratt | (pending) | (pending) | (pending) | (pending) |
| dpcmopt | Levinson-Durbin / Yule-Walker (textbook) | (pending) | (pending) | (pending) | (pending) |
| polystab / polyscale | Oppenheim & Schafer | (pending) | (pending) | (pending) | (pending) |
| scaleFilterSections | Jackson, Digital Filters & Signal Processing | (pending) | (pending) | (pending) | (pending) |
| fir2 | Oppenheim & Schafer (frequency sampling) | (pending) | (pending) | (pending) | (pending) |

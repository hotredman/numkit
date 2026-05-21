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
| pitchSRH | Drugman & Alwan 2011 (Interspeech) | cleanroom/specs/pitchSRH.md | clean-room agent (per spec) | (this commit) | gtest 16/16; parity OK — paper-faithful, srh_first intentionally not MATLAB-matched (re-baselined) |
| pitchPEF | Gonzalez & Brookes 2011 (EUSIPCO) | cleanroom/specs/pitchPEF.md | clean-room agent (per spec) | (this commit) | gtest 16/16; parity OK — paper-faithful (clean tone ~0.06% vs MATLAB; pef_r_first re-baselined) |
| designMelFilterBankSlaney | Davis & Mermelstein 1980; Slaney 1998 | cleanroom/specs/designMelFilterBankSlaney.md | clean-room agent (per spec) | (this commit) | gtest 20/20; parity correctness=OK (bit-identical to MATLAB) |
| adapthisteq | Zuiderveld 1994 (Graphics Gems IV); Pizer et al. 1990 / 1987 | cleanroom/specs/adapthisteq.md | clean-room agent (per spec) | (this commit) | gtest 16/16 (incl. 3 MATLAB-independent property tests); parity correctness=OK — clean-room CLAHE functionally equivalent, not bit-identical (interior pixels re-baselined; shape + saturated corners + low-contrast spread kept in fingerprint) |
| bwmorph | Lam/Lee/Suen 1992; Pratt | (pending) | (pending) | (pending) | (pending) |
| dpcmopt | Levinson-Durbin / Yule-Walker (textbook) | (pending) | (pending) | (pending) | (pending) |
| polystab / polyscale | Oppenheim & Schafer 3e §3.2/§5.6; Markel & Gray 1976; Hayes 1996 | cleanroom/specs/polystab_polyscale.md | clean-room agent (per spec) | (this commit) | gtest 18/18 (incl. 3 MATLAB-independent property tests); parity signal_polyscale + signal_polystab correctness=OK |
| scaleFilterSections | Jackson, Digital Filters & Signal Processing 1996; O&S 3e §6.3 | cleanroom/specs/scaleFilterSections.md | clean-room agent (per spec) | (this commit) | gtest 11/11 CtfUtilsTest (incl. MATLAB-independent cascade-product test); parity signal_scalefiltersections correctness=OK — bit-equal MATLAB R2025b incl. complex coefficients |
| fir2 | Oppenheim & Schafer 3e §7.4-7.5; Rabiner & Gold 1975; Parks & Burrus 1987 | cleanroom/specs/fir2.md | clean-room agent (per spec) | (this commit) | gtest 9/9 Fir2Test (incl. MATLAB-independent response test); parity signal_fir2 correctness=OK — bit-equal MATLAB R2025b on 20 fingerprints incl. npt/lap/window/odd-order |

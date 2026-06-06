# Contributing to numkit

Contributions are welcome.

## Licensing of contributions

numkit is **source-available** and dual-licensed (PolyForm
Noncommercial License for noncommercial use, and a separate paid
license for commercial use — see [LICENSE](LICENSE) and
[LICENSE-COMMERCIAL.md](LICENSE-COMMERCIAL.md)).

So that the project can keep offering both, **every contribution is
accepted under the [Contributor License Agreement](CLA.md)**. By
opening a pull request — or otherwise submitting a contribution — you
agree to the terms of `CLA.md`. Please read it before contributing.

If you are not able to agree to the CLA, please open an issue to
discuss before sending code.

## Working on the code

See [CLAUDE.md](CLAUDE.md) for build presets, test runners, and
repository conventions.

## Public API conventions

Every public function in `libs/<ns>/include/numkit/<ns>/**` must follow
the signature rules in [dev/LIBRARY_API.md](dev/LIBRARY_API.md) —
argument order, native scalar types, `const Value &` vs
`Span<const double>`, `FnHandle` callbacks, no `Engine *` in the public
API, multi-output return shape, and Doxygen requirements. Read it before
adding or refactoring a public function.

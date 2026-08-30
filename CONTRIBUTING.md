# Contributing to numkit

Contributions are welcome.

## Licensing of contributions
 
numkit is open-source software released under the [BSD Zero Clause License (0BSD)](LICENSE).
 
By submitting a pull request or contributing code to this repository, you agree that your contributions are licensed under the terms of the 0BSD license.
 
## Working on the code

See [AGENTS.md](AGENTS.md) for build presets, test runners, and
repository conventions.

## Public API conventions

Every public function in `toolboxes/<ns>/include/numkit/<ns>/**` must follow
the signature rules in [dev-docs/LIBRARY_API.md](dev-docs/LIBRARY_API.md) —
argument order, native scalar types, `const Value &` vs
`Span<const double>`, `FnHandle` callbacks, no `Engine *` in the public
API, multi-output return shape, and Doxygen requirements. Read it before
adding or refactoring a public function.

# PVS-Studio Static Analysis Report

> **Commit:** [`d7de9b197`] (branch `main`) | **Updated:** 2026-09-14 13:28 UTC

## Summary

| Severity | Count | Description |
| :--- | :---: | :--- |
| 🔴 **High** | **1** | Critical issues: null dereference, buffer overflow, UB |
| 🟡 **Medium** | **24** | Suspicious logic, potential leaks, unhandled branches |
| 🟢 **Low** | **56** | Code style, minor performance tips, informational |
| **Total** | **81** | |

### Warnings by Subsystem

| Subsystem | High | Medium | Low | Total |
| :--- | :---: | :---: | :---: | :---: |
| `core` | 1 | 23 | 54 | **78** |
| `other` | 0 | 1 | 2 | **3** |

---

## 🔴 High Severity Warnings

| Rule | CWE | Location | Message |
| :--- | :--- | :--- | :--- |
| [V836](https://pvs-studio.com/en/docs/warnings/v836/) | - | [`src/core/src/compiler.cpp:1195`](../../src/core/src/compiler.cpp#L1195) | Expression's value is copied at the 'savedConsts' variable declaration. The variable is never modified. |


## 🟡 Medium Severity Warnings

| Rule | CWE | Location | Message |
| :--- | :--- | :--- | :--- |
| [V1071](https://pvs-studio.com/en/docs/warnings/v1071/) | - | [`src/core/include/nk/core/compiler.hpp:221`](../../src/core/include/nk/core/compiler.hpp#L221) | Consider inspecting the 'tempReg' function. The return value is not always used. Total calls: 121, discarded results: 9. |
| [V1071](https://pvs-studio.com/en/docs/warnings/v1071/) | - | [`src/core/include/nk/core/compiler.hpp:248`](../../src/core/include/nk/core/compiler.hpp#L248) | Consider inspecting the 'compileNode' function. The return value is not always used. Total calls: 104, discarded results: 9. |
| [V730](https://pvs-studio.com/en/docs/warnings/v730/) | [CWE-457](https://cwe.mitre.org/data/definitions/457.html) | [`src/core/include/nk/core/debugger.hpp:112`](../../src/core/include/nk/core/debugger.hpp#L112) | Not all members of a class are initialized inside the compiler generated constructor. Consider inspecting: id, line. |
| [V1071](https://pvs-studio.com/en/docs/warnings/v1071/) | - | [`src/core/include/nk/core/parser.hpp:58`](../../src/core/include/nk/core/parser.hpp#L58) | Consider inspecting the 'match' function. The return value is not always used. Total calls: 17, discarded results: 1. |
| [V1048](https://pvs-studio.com/en/docs/warnings/v1048/) | [CWE-1164](https://cwe.mitre.org/data/definitions/1164.html) | [`src/core/src/compiler.cpp:1885`](../../src/core/src/compiler.cpp#L1885) | The 'stepReg' variable was assigned the same value. |
| [V560](https://pvs-studio.com/en/docs/warnings/v560/) | [CWE-571](https://cwe.mitre.org/data/definitions/571.html) | [`src/core/src/compiler.cpp:849`](../../src/core/src/compiler.cpp#L849) | A part of conditional expression is always true: !node->lhsTargets.empty(). |
| [V560](https://pvs-studio.com/en/docs/warnings/v560/) | [CWE-571](https://cwe.mitre.org/data/definitions/571.html) | [`src/core/src/compiler.cpp:3556`](../../src/core/src/compiler.cpp#L3556) | A part of conditional expression is always true: nargs > 0. |
| [V571](https://pvs-studio.com/en/docs/warnings/v571/) | [CWE-571](https://cwe.mitre.org/data/definitions/571.html) | [`src/core/src/compiler.cpp:4595`](../../src/core/src/compiler.cpp#L4595) | Recurring check. The 'ins.d < (int16_t) chunk.constants.size()' condition was already verified in line 4594. |
| [V730](https://pvs-studio.com/en/docs/warnings/v730/) | [CWE-457](https://cwe.mitre.org/data/definitions/457.html) | [`src/core/src/compiler.cpp:2798`](../../src/core/src/compiler.cpp#L2798) | Not all members of a class are initialized inside the compiler generated constructor. Consider inspecting: kind, node. |
| [V1037](https://pvs-studio.com/en/docs/warnings/v1037/) | [CWE-691](https://cwe.mitre.org/data/definitions/691.html) | [`src/core/src/dependency_graph.cpp:176`](../../src/core/src/dependency_graph.cpp#L176) | Two or more case-branches perform the same actions. Check lines: 176, 179 |
| [V1048](https://pvs-studio.com/en/docs/warnings/v1048/) | [CWE-1164](https://cwe.mitre.org/data/definitions/1164.html) | [`src/core/src/engine.cpp:2752`](../../src/core/src/engine.cpp#L2752) | The 'node->suppressOutput' variable was assigned the same value. |
| [V1048](https://pvs-studio.com/en/docs/warnings/v1048/) | [CWE-1164](https://cwe.mitre.org/data/definitions/1164.html) | [`src/core/src/parser.cpp:17`](../../src/core/src/parser.cpp#L17) | The 'eof.type' variable was assigned the same value. |
| [V1037](https://pvs-studio.com/en/docs/warnings/v1037/) | [CWE-691](https://cwe.mitre.org/data/definitions/691.html) | [`src/core/src/vm.cpp:2768`](../../src/core/src/vm.cpp#L2768) | Two or more case-branches perform the same actions. Check lines: 2768, 2771 |
| [V1048](https://pvs-studio.com/en/docs/warnings/v1048/) | [CWE-1164](https://cwe.mitre.org/data/definitions/1164.html) | [`src/core/src/vm.cpp:811`](../../src/core/src/vm.cpp#L811) | The 'fs.rangeType' variable was assigned the same value. |
| [V1048](https://pvs-studio.com/en/docs/warnings/v1048/) | [CWE-1164](https://cwe.mitre.org/data/definitions/1164.html) | [`src/core/src/vm.cpp:851`](../../src/core/src/vm.cpp#L851) | The 'fs.rangeType' variable was assigned the same value. |
| [V730](https://pvs-studio.com/en/docs/warnings/v730/) | [CWE-457](https://cwe.mitre.org/data/definitions/457.html) | [`src/core/src/vm.cpp:211`](../../src/core/src/vm.cpp#L211) | Not all members of a class are initialized inside the compiler generated constructor. Consider inspecting: reentrant. |
| [V812](https://pvs-studio.com/en/docs/warnings/v812/) | - | [`src/core/include/nk/core/engine.hpp:313`](../../src/core/include/nk/core/engine.hpp#L313) | Decreased performance. Ineffective use of the 'count' function. It can possibly be replaced by the call to the 'find' function. |
| [V802](https://pvs-studio.com/en/docs/warnings/v802/) | - | [`src/core/src/compiler.cpp:2799`](../../src/core/src/compiler.cpp#L2799) | On 64-bit platform, structure size can be reduced from 24 to 16 bytes by rearranging the fields according to their sizes in decreasing order. |
| [V833](https://pvs-studio.com/en/docs/warnings/v833/) | - | [`src/core/src/compiler.cpp:4053`](../../src/core/src/compiler.cpp#L4053) | Passing the const-qualified object 'entry.second' to the 'std::move' function disables move semantics. |
| [V812](https://pvs-studio.com/en/docs/warnings/v812/) | - | [`src/core/src/engine.cpp:2376`](../../src/core/src/engine.cpp#L2376) | Decreased performance. Ineffective use of the 'count' function. It can possibly be replaced by the call to the 'find' function. |
| [V820](https://pvs-studio.com/en/docs/warnings/v820/) | - | [`src/core/src/engine.cpp:233`](../../src/core/src/engine.cpp#L233) | The 'hdr' variable is not used after copying. Copying can be replaced with move/swap for optimization. |
| [V820](https://pvs-studio.com/en/docs/warnings/v820/) | - | [`src/core/src/engine.cpp:2323`](../../src/core/src/engine.cpp#L2323) | The 'userPath' variable is not used after copying. Copying can be replaced with move/swap for optimization. |
| [V833](https://pvs-studio.com/en/docs/warnings/v833/) | - | [`src/core/src/engine.cpp:2573`](../../src/core/src/engine.cpp#L2573) | Passing the const-qualified object 'entry.second' to the 'std::move' function disables move semantics. |
| [V820](https://pvs-studio.com/en/docs/warnings/v820/) | - | [`src/figure/include/nk/figure/figure_manager.hpp:821`](../../src/figure/include/nk/figure/figure_manager.hpp#L821) | The 'savedColorScale' variable is not used after copying. Copying can be replaced with move/swap for optimization. |


<details>
<summary><b>🟢 Low Severity Warnings (56)</b></summary>

| Rule | Location | Message |
| :--- | :--- | :--- |
| [V524](https://pvs-studio.com/en/docs/warnings/v524/) | `src/core/include/nk/core/debug_session.hpp:178` | It is odd that the body of 'onBreakpoint' function is fully equivalent to the body of 'onLine' function. |
| [V1096](https://pvs-studio.com/en/docs/warnings/v1096/) | `src/core/include/nk/core/vm.hpp:156` | The 'kEmpty' variable with static storage duration is declared inside the inline function with external linkage. This may lead to ODR violation. |
| [V1096](https://pvs-studio.com/en/docs/warnings/v1096/) | `src/core/include/nk/core/vm.hpp:218` | The 'kEmpty' variable with static storage duration is declared inside the inline function with external linkage. This may lead to ODR violation. |
| [V1029](https://pvs-studio.com/en/docs/warnings/v1029/) | `src/core/src/compiler.cpp:124` | Numeric Truncation Error. Return value of the 'size' function is written to the 16-bit variable. |
| [V1029](https://pvs-studio.com/en/docs/warnings/v1029/) | `src/core/src/compiler.cpp:282` | Numeric Truncation Error. Return value of the 'size' function is written to the 16-bit variable. |
| [V1029](https://pvs-studio.com/en/docs/warnings/v1029/) | `src/core/src/compiler.cpp:442` | Numeric Truncation Error. Return value of the 'size' function is written to the 16-bit variable. |
| [V1029](https://pvs-studio.com/en/docs/warnings/v1029/) | `src/core/src/compiler.cpp:453` | Numeric Truncation Error. Return value of the 'size' function is written to the 16-bit variable. |
| [V1029](https://pvs-studio.com/en/docs/warnings/v1029/) | `src/core/src/compiler.cpp:496` | Numeric Truncation Error. Return value of the 'size' function is written to the 16-bit variable. |
| [V1029](https://pvs-studio.com/en/docs/warnings/v1029/) | `src/core/src/compiler.cpp:505` | Numeric Truncation Error. Return value of the 'size' function is written to the 16-bit variable. |
| [V1029](https://pvs-studio.com/en/docs/warnings/v1029/) | `src/core/src/compiler.cpp:666` | Numeric Truncation Error. Return value of the 'size' function is written to the 16-bit variable. |
| [V1029](https://pvs-studio.com/en/docs/warnings/v1029/) | `src/core/src/compiler.cpp:3709` | Numeric Truncation Error. Return value of the 'size' function is written to the 16-bit variable. |
| [V1029](https://pvs-studio.com/en/docs/warnings/v1029/) | `src/core/src/compiler.cpp:3824` | Numeric Truncation Error. Return value of the 'size' function is written to the 16-bit variable. |
| [V1029](https://pvs-studio.com/en/docs/warnings/v1029/) | `src/core/src/compiler.cpp:3839` | Numeric Truncation Error. Return value of the 'size' function is written to the 16-bit variable. |
| [V1029](https://pvs-studio.com/en/docs/warnings/v1029/) | `src/core/src/compiler.cpp:3884` | Numeric Truncation Error. Return value of the 'size' function is written to the 8-bit variable. |
| [V1029](https://pvs-studio.com/en/docs/warnings/v1029/) | `src/core/src/compiler.cpp:4079` | Numeric Truncation Error. Return value of the 'size' function is written to the 8-bit variable. |
| [V1029](https://pvs-studio.com/en/docs/warnings/v1029/) | `src/core/src/compiler.cpp:4080` | Numeric Truncation Error. Return value of the 'size' function is written to the 8-bit variable. |
| [V1037](https://pvs-studio.com/en/docs/warnings/v1037/) | `src/core/src/compiler.cpp:2751` | Two or more case-branches perform the same actions. Check lines: 2751, 2756 |
| [V522](https://pvs-studio.com/en/docs/warnings/v522/) | `src/core/src/debug_session.cpp:294` | There might be dereferencing of a potential null pointer 'ctl->currentFrame()'. |
| [V522](https://pvs-studio.com/en/docs/warnings/v522/) | `src/core/src/debug_session.cpp:413` | There might be dereferencing of a potential null pointer 'ctl->currentFrame()'. |
| [V524](https://pvs-studio.com/en/docs/warnings/v524/) | `src/core/src/engine.cpp:662` | It is odd that the body of 'accessWord' function is fully equivalent to the body of 'accessName' function. |
| [V565](https://pvs-studio.com/en/docs/warnings/v565/) | `src/core/src/engine.cpp:1974` | An empty exception handler. Silent suppression of exceptions can hide the presence of bugs in source code during testing. |
| [V565](https://pvs-studio.com/en/docs/warnings/v565/) | `src/core/src/engine.cpp:2312` | An empty exception handler. Silent suppression of exceptions can hide the presence of bugs in source code during testing. |
| [V565](https://pvs-studio.com/en/docs/warnings/v565/) | `src/core/src/engine.cpp:2351` | An empty exception handler. Silent suppression of exceptions can hide the presence of bugs in source code during testing. |
| [V524](https://pvs-studio.com/en/docs/warnings/v524/) | `src/core/src/environment.cpp:130` | It is odd that the body of 'setLocalFast' function is fully equivalent to the body of 'setLocal' function. |
| [V1048](https://pvs-studio.com/en/docs/warnings/v1048/) | `src/core/src/parser.cpp:20` | The 'eof.col' variable was assigned the same value. |
| [V522](https://pvs-studio.com/en/docs/warnings/v522/) | `src/core/src/parser.cpp:510` | There might be dereferencing of a potential null pointer 'expr'. |
| [V1029](https://pvs-studio.com/en/docs/warnings/v1029/) | `src/core/src/vm.cpp:2202` | Numeric Truncation Error. Return value of the 'size' function is written to the 8-bit variable. |
| [V1029](https://pvs-studio.com/en/docs/warnings/v1029/) | `src/core/src/vm.cpp:2419` | Numeric Truncation Error. Return value of the 'size' function is written to the 8-bit variable. |
| [V1029](https://pvs-studio.com/en/docs/warnings/v1029/) | `src/core/src/vm.cpp:2499` | Numeric Truncation Error. Return value of the 'size' function is written to the 8-bit variable. |
| [V1029](https://pvs-studio.com/en/docs/warnings/v1029/) | `src/core/src/vm.cpp:2670` | Numeric Truncation Error. Return value of the 'size' function is written to the 8-bit variable. |
| [V1037](https://pvs-studio.com/en/docs/warnings/v1037/) | `src/core/src/vm.cpp:3176` | Two or more case-branches perform the same actions. Check lines: 3176, 3188 |
| [V1037](https://pvs-studio.com/en/docs/warnings/v1037/) | `src/core/src/vm.cpp:3177` | Two or more case-branches perform the same actions. Check lines: 3177, 3189 |
| [V1037](https://pvs-studio.com/en/docs/warnings/v1037/) | `src/core/src/vm.cpp:3178` | Two or more case-branches perform the same actions. Check lines: 3178, 3190 |
| [V1037](https://pvs-studio.com/en/docs/warnings/v1037/) | `src/core/src/vm.cpp:3179` | Two or more case-branches perform the same actions. Check lines: 3179, 3191 |
| [V1037](https://pvs-studio.com/en/docs/warnings/v1037/) | `src/core/src/vm.cpp:3181` | Two or more case-branches perform the same actions. Check lines: 3181, 3192 |
| [V1037](https://pvs-studio.com/en/docs/warnings/v1037/) | `src/core/src/vm.cpp:3193` | Two or more case-branches perform the same actions. Check lines: 3193, 3196 |
| [V1037](https://pvs-studio.com/en/docs/warnings/v1037/) | `src/core/src/vm.cpp:3198` | Two or more case-branches perform the same actions. Check lines: 3198, 3199 |
| [V1048](https://pvs-studio.com/en/docs/warnings/v1048/) | `src/core/src/vm.cpp:357` | The 'cf.regBase' variable was assigned the same value. |
| [V1048](https://pvs-studio.com/en/docs/warnings/v1048/) | `src/core/src/vm.cpp:359` | The 'cf.forStackBase' variable was assigned the same value. |
| [V1048](https://pvs-studio.com/en/docs/warnings/v1048/) | `src/core/src/vm.cpp:360` | The 'cf.tryStackBase' variable was assigned the same value. |
| [V1048](https://pvs-studio.com/en/docs/warnings/v1048/) | `src/core/src/vm.cpp:805` | The 'fs.index' variable was assigned the same value. |
| [V1048](https://pvs-studio.com/en/docs/warnings/v1048/) | `src/core/src/vm.cpp:809` | The 'fs.rows' variable was assigned the same value. |
| [V1048](https://pvs-studio.com/en/docs/warnings/v1048/) | `src/core/src/vm.cpp:810` | The 'fs.data' variable was assigned the same value. |
| [V1048](https://pvs-studio.com/en/docs/warnings/v1048/) | `src/core/src/vm.cpp:813` | The 'fs.count' variable was assigned the same value. |
| [V1048](https://pvs-studio.com/en/docs/warnings/v1048/) | `src/core/src/vm.cpp:848` | The 'fs.index' variable was assigned the same value. |
| [V522](https://pvs-studio.com/en/docs/warnings/v522/) | `src/core/src/vm.cpp:2246` | There might be dereferencing of a potential null pointer 'AB'. |
| [V522](https://pvs-studio.com/en/docs/warnings/v522/) | `src/core/src/vm.cpp:2428` | There might be dereferencing of a potential null pointer 'AB'. |
| [V522](https://pvs-studio.com/en/docs/warnings/v522/) | `src/core/src/vm.cpp:2516` | There might be dereferencing of a potential null pointer 'argPtr'. |
| [V553](https://pvs-studio.com/en/docs/warnings/v553/) | `src/core/src/vm.cpp:420` | The length of 'dispatchLoop' function's body is more than 2000 lines long. You should consider refactoring the code. |
| [V656](https://pvs-studio.com/en/docs/warnings/v656/) | `src/ops/include/nk/ops/rng_context.hpp:81` | Variables 'v1', 'v2' are initialized through the call to the same function. It's probably an error or un-optimized code. Consider inspecting the '2.0 * v4Uniform() - 1.0' expression. Check lines: 80, 81. |
| [V524](https://pvs-studio.com/en/docs/warnings/v524/) | `src/value/include/nk/value/value.hpp:682` | It is odd that the body of 'isHeap' function is fully equivalent to the body of 'hasHeap' function. |
| [V802](https://pvs-studio.com/en/docs/warnings/v802/) | `src/core/include/nk/core/vm.hpp:301` | On 64-bit platform, structure size can be reduced from 88 to 80 bytes by rearranging the fields according to their sizes in decreasing order. |
| [V820](https://pvs-studio.com/en/docs/warnings/v820/) | `src/core/src/environment.cpp:162` | The 'newParent' variable is not used after copying. Copying can be replaced with move/swap for optimization. |
| [V815](https://pvs-studio.com/en/docs/warnings/v815/) | `src/core/src/parser.cpp:18` | Decreased performance. Consider replacing the expression 'eof.value = ""' with 'eof.value.clear()'. |
| [V821](https://pvs-studio.com/en/docs/warnings/v821/) | `src/core/src/tree_walker.cpp:1074` | Decreased performance. The 'fname' variable can be constructed in a lower level scope. |
| [V821](https://pvs-studio.com/en/docs/warnings/v821/) | `src/core/src/vm.cpp:228` | Decreased performance. The 'restorer' variable can be constructed in a lower level scope. |

</details>

# todo: Numkit Model Context Protocol (MCP) Server for AI Agents

*Kind:* feature / ecosystem · *Status:* open · *Surfaced:* 2026-08-30

> Lifecycle: open → done. On completion, record the outcome in
> `dev-docs/memory/` (per the AGENTS.md project-memory protocol) and
> delete this file — the todo list holds open work only.

## Goal
Build the official, zero-dependency, ultra-fast **Numkit MCP Server** implementing the Model Context Protocol (MCP). This enables any modern AI agent (Claude Desktop, Cursor, Claude Code, Antigravity, Continue.dev, LangChain, AutoGen) to invoke NumKit as a native mathematical and numerical computing sandbox.

## Problem & Rationale
1. **LLM Mathematical Limitations**: LLMs frequently make arithmetic errors, struggle with eigenvalue decompositions, DSP filter design, matrix inversions, and high-order ODE simulations when doing mental calculations.
2. **Python Sandbox Overhead**: Current AI agent workflows rely on heavy Python/Jupyter Docker containers (500 MB+ footprint, 300–800 ms cold start, environment dependency drift).
3. **Numkit Advantage**: NumKit runs in-process via WebAssembly / C++ with < 1 ms startup, 5 MB footprint, memory safety (StackGuard, COW), and a 2–3× more token-efficient matrix DSL than Python.

## Architecture & Implementation Plan

### 1. Package Structure
- **Location:** `packages/numkit-mcp/`
- **Runtime:** TypeScript / Node.js leveraging `@modelcontextprotocol/sdk` on top of the bundled Numkit WASM engine (`packages/numkit`).
- **Transport:** Stdio (standard input/output for local agents) and SSE (for remote agent services).

### 2. Exposed MCP Tools

| Tool | Purpose | Parameters | Output |
|---|---|---|---|
| `numkit_eval` | Execute MATLAB/NumKit scripts | `code` (string), `reset_workspace` (bool, optional) | Formatted stdout, return values, variable metadata (dimensions, type), execution time (ms) |
| `numkit_matrix_solve` | Fast linear system solver ($A x = b$, eigen, SVD, inverse) | `operation` (string), `A` (2D array), `b` (1D/2D array, optional) | Solution matrix/vector, eigenvalues, condition number |
| `numkit_filter_design` | DSP digital filter design & response | `type` (`butter`/`cheby`/`fir`), `order`, `cutoff`, `sample_rate` | Filter coefficients (`b`, `a`), pole-zero summary, frequency response |
| `numkit_help` | In-context function documentation | `fn_name` (string) | MATLAB-compatible signature, options, and usage examples |

### 3. Execution Safety & Workspace Management
- **Stateful vs Stateless:** Support persistent workspace sessions (preserving variables like `x`, `A`, `model` across sequential conversation turns) with explicit `clear` / reset triggers.
- **Resource Limits:** Configurable execution timeout (default 3000 ms) and memory budget to protect agent runtimes against infinite loops.
- **Structured Error Diagnostics:** Transform runtime errors and syntax errors into actionable guidance for the LLM without crashing the MCP connection.

### 4. Agent Configuration & Distribution
- **NPM Package:** `@numkit/mcp-server` published to npmjs.com.
- **Zero-Install Invocation:**
  ```json
  {
    "mcpServers": {
      "numkit": {
        "command": "npx",
        "args": ["-y", "@numkit/mcp-server"]
      }
    }
  }
  ```
- **Local Antigravity Integration:** Register as a local workspace server in `.agents/` and create `.agents/skills/numkit-math/SKILL.md` to guide prompt routing.

## Acceptance Criteria
- [ ] Working MCP server with `stdio` transport passing official MCP validation tests.
- [ ] `numkit_eval` tool executes matrix math, linear algebra, DSP, and statistics with < 5 ms roundtrip latency.
- [ ] Verified live integration in Claude Desktop, Cursor, and Antigravity.
- [ ] Published to npm under `@numkit/mcp-server` (or `numkit-mcp`).

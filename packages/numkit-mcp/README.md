# numkit-mcp

MCP server exposing the [numkit](https://www.npmjs.com/package/numkit)
numerical computing engine as tools for AI agents.

## Why

AI agents (Claude Desktop, Cursor, Claude Code, Continue, AutoGen, LangChain)
need reliable numerical computation. Python/Jupyter sandboxes are heavy
(300–800 ms cold start, ~500 MB) and token-expensive. numkit-mcp provides:

- **Instant startup** (< 1 ms after first load — in-process WASM)
- **Persistent workspace** — define a function in one call, use it in the next
- **Token-efficient** — native MATLAB matrix syntax (`A \ b`, `fft(x)`, `[B,A] = butter(4, 0.2)`)
- **Zero dependencies** — the WASM engine ships inside the package

## Install

```json
{
  "mcpServers": {
    "numkit": {
      "command": "npx",
      "args": ["-y", "numkit-mcp"]
    }
  }
}
```

## Tools

| Tool | Purpose |
|---|---|
| `numkit_eval(code)` | Execute MATLAB/numkit code in a persistent workspace |
| `numkit_reset()` | Clear the workspace |
| `numkit_help(fn)` | Get function documentation |

## Example session

```
Agent: numkit_eval("A = [1 2; 3 4]; b = [5; 11]; x = A \ b")
→ x = [-3; 4]

Agent: numkit_eval("eig(A)")
→ eigenvalues of A (workspace survived from the previous call)

Agent: numkit_eval("[B, A] = butter(4, 0.2); freqz(B, A)")
→ filter design + frequency response
```

## Configuration

| Env var | Default | Description |
|---|---|---|
| `NUMKIT_MCP_TIMEOUT` | `3000` | Max execution time per call (ms) |

## NOT a filesystem sandbox

Like any scripting-language CLI, scripts can read and write files in the
working directory. Do not use this to execute untrusted code.

## License

0BSD (same as numkit).

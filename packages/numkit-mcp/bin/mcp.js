#!/usr/bin/env node
// packages/numkit-mcp/bin/mcp.js
//
// Numkit MCP server — exposes the numkit WASM engine as MCP tools for
// AI agents. Zero dependencies: hand-rolled stdio JSON-RPC.
//
// Tools:
//   numkit_eval(code)     — execute MATLAB code in a PERSISTENT workspace
//   numkit_reset()        — clear the workspace
//   numkit_help(fn)       — function documentation from the help catalog
//
// Protocol: MCP over stdio (JSON-RPC 2.0). Each message is a
// newline-delimited JSON object on stdin; responses on stdout.

"use strict";

const readline = require("readline");
const { loadEngine, stripIdeMarkers, failedAt } = require("numkit/bin/engine");

// ── Configuration ─────────────────────────────────────────────────────────

const TIMEOUT_MS = parseInt(process.env.NUMKIT_MCP_TIMEOUT || "3000", 10);
const SERVER_INFO = {
  name: "numkit",
  version: require("../package.json").version,
};

// ── Engine state (persistent across tool calls) ───────────────────────────

let engine = null;
let engineReady = false;

async function ensureEngine() {
  if (!engineReady) {
    engine = await loadEngine();
    engineReady = true;
  }
  return engine;
}

// ── Tool implementations ──────────────────────────────────────────────────

async function toolEval(code) {
  const mod = await ensureEngine();
  const t0 = Date.now();

  let out;
  try {
    out = stripIdeMarkers(mod.repl_execute(code));
  } catch (e) {
    return {
      content: [{ type: "text", text: `Engine crashed: ${e.message}` }],
      isError: true,
    };
  }

  const durationMs = Date.now() - t0;
  const idx = failedAt(out);

  if (idx !== -1) {
    return {
      content: [{
        type: "text",
        text: `Error (in ${durationMs} ms):\n${out.slice(idx).trim()}`,
      }],
      isError: true,
      _meta: { durationMs },
    };
  }

  // Clean output — trim trailing newline for the response
  const text = out.trim();
  return {
    content: [{
      type: "text",
      text: text.length > 0 ? text : "(no output)",
    }],
    isError: false,
    _meta: { durationMs },
  };
}

async function toolReset() {
  const mod = await ensureEngine();
  mod.repl_execute("clear;");
  mod.repl_execute("__CLEAR__");
  return {
    content: [{ type: "text", text: "Workspace cleared." }],
    isError: false,
  };
}

async function toolHelp(fn) {
  const mod = await ensureEngine();
  const out = stripIdeMarkers(mod.repl_execute(`help ${fn}`));
  const idx = failedAt(out);
  if (idx !== -1) {
    return {
      content: [{ type: "text", text: `No help found for '${fn}'.` }],
      isError: true,
    };
  }
  return {
    content: [{ type: "text", text: out.trim() || "(no help text)" }],
    isError: false,
  };
}

// ── Tool schemas ──────────────────────────────────────────────────────────

const TOOLS = [
  {
    name: "numkit_eval",
    description:
      "Execute MATLAB/numkit code in a persistent workspace. " +
      "The workspace survives across calls — variables and functions " +
      "defined in one call are available in the next. " +
      "NOT a filesystem sandbox: scripts can read/write the working directory.",
    inputSchema: {
      type: "object",
      properties: {
        code: {
          type: "string",
          description: "MATLAB/numkit code to execute (statement(s) or expression).",
        },
      },
      required: ["code"],
    },
  },
  {
    name: "numkit_reset",
    description: "Clear the persistent workspace (all variables and functions).",
    inputSchema: { type: "object", properties: {}, },
  },
  {
    name: "numkit_help",
    description: "Get documentation for a numkit/MATLAB function.",
    inputSchema: {
      type: "object",
      properties: {
        fn: {
          type: "string",
          description: "Function name (e.g. 'fft', 'butter', 'ode45').",
        },
      },
      required: ["fn"],
    },
  },
];

// ── JSON-RPC dispatch ─────────────────────────────────────────────────────

async function dispatch(method, params, id) {
  switch (method) {
  case "initialize":
    return {
      protocolVersion: "2024-11-05",
      capabilities: { tools: {} },
      serverInfo: SERVER_INFO,
    };

  case "notifications/initialized":
    return null; // notification, no response

  case "tools/list":
    return { tools: TOOLS };

  case "tools/call": {
    const name = params.name;
    const args = params.arguments || {};

    // Watchdog: race the tool against a timeout
    const timeoutPromise = new Promise((_, reject) =>
      setTimeout(() => reject(new Error(`numkit_eval timed out after ${TIMEOUT_MS} ms`)), TIMEOUT_MS));

    let result;
    try {
      let work;
      if (name === "numkit_eval") {
        work = toolEval(args.code || "");
      } else if (name === "numkit_reset") {
        work = toolReset();
      } else if (name === "numkit_help") {
        work = toolHelp(args.fn || "");
      } else {
        return {
          jsonrpc: "2.0", id,
          error: { code: -32602, message: `Unknown tool: ${name}` },
        };
      }
      result = await Promise.race([work, timeoutPromise]);
    } catch (e) {
      result = {
        content: [{ type: "text", text: e.message }],
        isError: true,
      };
    }

    return {
      jsonrpc: "2.0", id,
      result: {
        content: result.content,
        isError: result.isError || false,
      },
    };
  }

  case "ping":
    return {};

  default:
    return { error: { code: -32601, message: `Method not found: ${method}` } };
  }
}

// ── Stdio transport ───────────────────────────────────────────────────────

const rl = readline.createInterface({
  input: process.stdin,
  terminal: false,
});

rl.on("line", async (line) => {
  line = line.trim();
  if (!line) return;

  let msg;
  try {
    msg = JSON.parse(line);
  } catch {
    process.stderr.write(`numkit-mcp: invalid JSON: ${line.slice(0, 80)}\n`);
    return;
  }

  const { method, params, id } = msg;
  if (!method) return;

  try {
    const result = await dispatch(method, params, id);
    // Notifications (no id) don't get a response
    if (id !== undefined && id !== null && result !== null) {
      const response = result.jsonrpc ? result : { jsonrpc: "2.0", id, result };
      process.stdout.write(JSON.stringify(response) + "\n");
    }
  } catch (e) {
    if (id !== undefined && id !== null) {
      process.stdout.write(
        JSON.stringify({
          jsonrpc: "2.0", id,
          error: { code: -32603, message: e.message },
        }) + "\n",
      );
    }
  }
});

rl.on("close", () => {
  process.exit(0);
});

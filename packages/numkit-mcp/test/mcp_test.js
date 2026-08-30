// packages/numkit-mcp/test/mcp_test.js
//
// End-to-end MCP server tests: spawn the server, speak JSON-RPC over
// stdio, verify handshake + all three tools + persistence + watchdog.
//
// Run: node test/mcp_test.js

"use strict";

const { spawn } = require("child_process");
const path = require("path");
const readline = require("readline");

const SERVER = path.join(__dirname, "..", "bin", "mcp.js");

let passed = 0, failed = 0;
function ok(name, cond, detail) {
  if (cond) { passed++; console.log(`  ✓ ${name}`); }
  else { failed++; console.log(`  ✗ ${name}${detail ? ": " + detail : ""}`); }
}

// ── Test harness: spawn server, send messages, collect responses ─────────

class McpClient {
  constructor() {
    this.proc = spawn("node", [SERVER], { stdio: ["pipe", "pipe", "pipe"] });
    this.rl = readline.createInterface({ input: this.proc.stdout, terminal: false });
    this.pending = new Map();
    this.nextId = 1;
    this.rl.on("line", (line) => {
      try {
        const msg = JSON.parse(line);
        if (msg.id !== undefined && this.pending.has(msg.id)) {
          this.pending.get(msg.id)(msg);
          this.pending.delete(msg.id);
        }
      } catch {}
    });
  }

  send(method, params) {
    return new Promise((resolve) => {
      const id = this.nextId++;
      this.pending.set(id, resolve);
      const msg = { jsonrpc: "2.0", id, method, params };
      this.proc.stdin.write(JSON.stringify(msg) + "\n");
    });
  }

  notify(method, params) {
    const msg = { jsonrpc: "2.0", method, params };
    this.proc.stdin.write(JSON.stringify(msg) + "\n");
  }

  close() {
    this.proc.stdin.end();
    this.proc.kill();
  }
}

// ── Tests ─────────────────────────────────────────────────────────────────

async function runTests() {
  console.log("numkit-mcp test suite\n");
  const c = new McpClient();

  // 1. Handshake
  console.log("Handshake:");
  const init = await c.send("initialize", {
    protocolVersion: "2024-11-05",
    capabilities: {},
    clientInfo: { name: "test", version: "0.0.1" },
  });
  ok("initialize returns serverInfo", init.result && init.result.serverInfo);
  ok("serverInfo.name == numkit", init.result.serverInfo.name === "numkit");
  ok("protocol version present", init.result.protocolVersion);

  c.notify("notifications/initialized");

  // 2. tools/list
  console.log("\nTools:");
  const tools = await c.send("tools/list", {});
  const names = tools.result.tools.map((t) => t.name);
  ok("3 tools listed", names.length === 3, `got ${names.length}`);
  ok("numkit_eval present", names.includes("numkit_eval"));
  ok("numkit_reset present", names.includes("numkit_reset"));
  ok("numkit_help present", names.includes("numkit_help"));

  // 3. numkit_eval: basic computation
  console.log("\nnumkit_eval:");
  const eval1 = await c.send("tools/call", {
    name: "numkit_eval",
    arguments: { code: "disp(2 + 3)" },
  });
  const eval1Text = eval1.result.content[0].text;
  ok("2+3=5", eval1Text.includes("5"), `got: ${eval1Text}`);
  ok("not an error", !eval1.result.isError);

  // 4. Persistence: define in call 1, use in call 2
  console.log("\nPersistence:");
  await c.send("tools/call", {
    name: "numkit_eval",
    arguments: { code: "x = 42; f = @(n) n * 2;" },
  });
  const eval2 = await c.send("tools/call", {
    name: "numkit_eval",
    arguments: { code: "disp(x); disp(f(10))" },
  });
  const eval2Text = eval2.result.content[0].text;
  ok("x survives (42)", eval2Text.includes("42"), `got: ${eval2Text}`);
  ok("f(10)=20 survives", eval2Text.includes("20"), `got: ${eval2Text}`);

  // 5. Error handling
  console.log("\nErrors:");
  const err = await c.send("tools/call", {
    name: "numkit_eval",
    arguments: { code: "undefined_fn_xyz(1)" },
  });
  ok("error flagged", err.result.isError);
  ok("error message present", err.result.content[0].text.includes("Error"));

  // 6. numkit_help
  console.log("\nnumkit_help:");
  const help = await c.send("tools/call", {
    name: "numkit_help",
    arguments: { fn: "fft" },
  });
  ok("fft help returned", !help.result.isError);
  ok("help has content", help.result.content[0].text.length > 10);

  // 7. numkit_reset
  console.log("\nnumkit_reset:");
  const reset = await c.send("tools/call", {
    name: "numkit_reset",
    arguments: {},
  });
  ok("reset succeeded", !reset.result.isError);
  const eval3 = await c.send("tools/call", {
    name: "numkit_eval",
    arguments: { code: "disp(exist('x'))" },
  });
  ok("x gone after reset", eval3.result.content[0].text.trim() === "0",
     `got: ${eval3.result.content[0].text}`);

  // 8. Matrix computation (broader engine test)
  console.log("\nMatrix:");
  const mat = await c.send("tools/call", {
    name: "numkit_eval",
    arguments: { code: "A = [1 2; 3 4]; d = det(A); disp(d)" },
  });
  ok("det([1 2;3 4]) = -2", mat.result.content[0].text.includes("-2"),
     `got: ${mat.result.content[0].text}`);

  // Cleanup
  c.close();

  // Summary
  console.log(`\n${"=".repeat(40)}`);
  console.log(`  ${passed} passed, ${failed} failed`);
  console.log(`${"=".repeat(40)}`);
  process.exit(failed > 0 ? 1 : 0);
}

runTests().catch((e) => {
  console.error("Test harness error:", e);
  process.exit(1);
});

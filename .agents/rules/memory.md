---
description: Instructs the agent on how to manage and use the project memory system.
trigger: always_on
---

# Project Memory System

This project maintains a detailed, decentralized memory system located in `dev-docs/memory/`.

## Purpose
The memory system is used to persist architectural decisions, context on bugs/gotchas, performance metrics, and workflow details across agent sessions.

## Agent Instructions
1. **Context Retrieval**: When starting a new major task, check `dev-docs/memory/` for any files relevant to the components you are working on. You can use the `list_dir` tool to see available memory files.
2. **Recording Decisions**: When you complete a major task, make significant architectural decisions, or discover a non-obvious bug/gotcha, you **MUST** document it in `dev-docs/memory/`.
3. **Format**: Create a new markdown file for a new logical task/domain (e.g., `linalg_lu_performance.md`, `memory_allocation_strategy.md`), or append to an existing one if it naturally fits.
4. **Content**: Memory files should include:
   - The problem or context.
   - The chosen solution or decision.
   - Why it was chosen (rationale).
   - Any quantitative results (e.g., benchmark numbers) if applicable.

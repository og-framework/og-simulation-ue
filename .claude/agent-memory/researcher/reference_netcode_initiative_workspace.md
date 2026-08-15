---
name: netcode-initiative-workspace
description: Where the og-netcode-v2-input-relay initiative keeps its briefs, findings, and conventions for design investigations
metadata:
  type: reference
---

Initiative workspace: `C:\Users\olle\Documents\Notes\AgentWorkspace\Initiatives\og-netcode-v2-input-relay`.

- `Backlog.md` — numbered items with in-place **Status:** blocks (edit the block, keep "Previously:" history).
- `current_state.md` — **prepend-newest** log (new entries go at the top, as a `> ##` blockquote block, dated, signed with model name).
- `impl/design_taskNN_*.md` / `impl/finding_taskNN_*.md` — deliverables; `impl/_dispatch_tNN.txt` — briefs.
- Design investigations end with: the doc, a Backlog status update, a current_state.md prepend, and a **zero-change proof** (`git diff --stat` in og-brawler-unreal AND the OGSimulation submodule, compared against a baseline captured at dispatch — capture it FIRST, there are usually pre-existing uncommitted user edits).

Conventions that briefs enforce (they will be checked in review):
- Cite engine/simulation claims **by symbol** (`Class::function`, file), never by line number.
- Label every derived number "derived"; keep measured/derived/assumed distinct.
- Worst-case arithmetic goes against the **packet** (`GetMaxSingleBunchSizeBits`, ~952 B usable of MAX_PACKET_SIZE=1024 — see design_task38 §3.1; the ~975 B figure in older docs is unpinned), never the rate allowance — this initiative's reusable lesson (finding_task37 §4).
- No .github/coding-style.md exists; conventions live in per-repo CONTRIBUTING.md. SPDX: BUSL-1.1 for Source/OGBrawlerUnreal, MPL-2.0 for og-simulation and og-simulation-tests.
- Canonical numbers live in `impl/finding_task37_depth_regression.md` (packet overflow diagnosis) and `impl/design_task38_input_first_replication.md` (payload sizes: state 311 B wire / 300 B composite; ring entry 81 B; per-batch overhead ~6-7 B). Verify against source before reusing — this initiative overturned 8+ confidently-stated claims by checking.

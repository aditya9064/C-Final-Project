# AI Use Log

Per the course AI-use policy, this log records significant AI tool use
across the project: who used it, where, why, what was kept/changed, how it
was checked, and what was learned. Add an entry per significant use as you
go — it's much harder to reconstruct this at the end.

Format (one entry per use):

```
### [Date] — [Person] — [Tool/Model]
- **Where:** file/function/report section
- **Why:** what problem or question prompted this
- **What I kept/changed:** summary of what was used vs. rewritten
- **How I checked it:** docs consulted, tests run, manual verification
- **What I learned:** the actual takeaway
- **Excerpt:** short prompt/response snippet (optional but encouraged)
```

---

### 2026-11-04 — Aditya Miriyala — GitHub Copilot (Copilot SDK in VS Code)
- **Where:** Project ideation; `docs/proposal.md`; initial repository
  scaffolding (`CMakeLists.txt`, `.github/` templates/CI, `README.md`).
- **Why:** Brainstormed and stress-tested project-scope options against
  the assignment's grading rubric before committing to an idea; drafted the
  proposal document; set up initial repo structure/tooling.
- **What I kept/changed:** Kept the overall project concept (networked
  tactical shooter with AI-filled lobby slots) and proposal structure after
  review; adjusted role assignments and scope split based on team input.
  Wrote and locally built/tested the CMake + SFML + ENet skeleton myself
  rather than trusting it blindly — see "How I checked it."
- **How I checked it:** Ran `cmake -S . -B build`, `cmake --build build`,
  `ctest`, and the smoke-test executable locally before committing, to
  confirm the fetched dependencies (SFML, ENet, doctest) actually configure,
  compile, link, and run rather than just "looking right."
- **What I learned:** ENet's own `CMakeLists.txt` uses directory-scoped
  `include_directories()` instead of `target_include_directories()`, so its
  headers don't automatically propagate to consuming targets in other
  directories — had to add `${enet_SOURCE_DIR}/include` explicitly to our
  targets.

<!-- Add further entries above this line as the project progresses. -->

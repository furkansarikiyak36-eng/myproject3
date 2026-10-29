# 2026-10-29 · claude-code · game plan
Branch: feature/simple-intent
## Done
- Wrote `Docs/GameDesign/MYPROJECT3_OYUN_PLANI_TR.md`: principles, primitive-era materials,
  38 transformations, systems, farming chain, calendar/events, exploration zones, repo status, phases 0-12.
- Decision summary in `DECISIONS.md`.
- `memory/ROADMAP.md`: ordered tasks P0-P12, honest status levels, resume protocol, open questions Q0-Q8.
- `memory/CODEBASE.md` (living analysis by plan area) + generated `memory/CODE_INDEX.md` (`tools/memory_code_index.py`).
## Found
- No code for seasons/months, spoilage, disease, flood, avalanche, wildfire, soil fertility.
- Agriculture is plant -> stages by time and moisture only.
- Audit: found 3 separate code lines; group-a-code-closure (ChatGPT, +182 on our base) does not compile (10 UHT + 6 C++ errors). Corrected map (bridges/zipline exist).
## Open
- Q00: fix and adopt group-a-code-closure?
- User approval of the plan and the 7 open questions (section 12).
- Phase 0: merge snap branch, move lists to `Config/` data.

# Current task (checkpoint)

**Every assistant reads this file FIRST.** If the status below is `open`, continue from
"Next step". Do **not** analyse again what "Findings" already says.

## Rules (the limit can end the session at any moment)

1. New task from the user: replace the section below (status `open`), write the goal and plan,
   save the file **before** reading any code.
2. After **every** step (one file read, one search, one edit): add 1–3 lines to "Findings" or
   "Changes", update "Next step", save. Small and often; never keep results only in the chat.
3. Read only what `memory/CODEBASE.md` points to. Write down in "Read" each file you opened, so
   nobody opens it again for the same question.
4. Finished: status `done`; move lasting facts to `CODEBASE.md` (Findings) and the session file;
   clear this file back to the empty template at the next new task.
5. Cannot write to the repo (read-only GitHub connector)? End **every** answer with the updated
   block of this file so the user can paste or commit it.

---

## Task

Status: `done`
Goal: Judge the quality of group-a-code-closure: can we take it as is?
Asked by the user: "chatgpt kurduğu dal kaliteli değil mi alamaz mıyız"
Started: 2026-10-29 · Last saved: 2026-10-29 · Branch: feature/simple-intent

### Plan
1. Tests added, player wiring (input/HUD callers), data vs hard-coded, complexity (receipts/schema).
2. Sample 2-3 functions.
3. Verdict + record in CODEBASE.md.

### Read
- —

### Findings
- Known: 10 UHT + 6 C++ errors (CODEBASE Findings).
- Tests 48 -> 65 core (authored). Player wiring: only LogSnapCharacter +7 lines (dig tool N); HUD, carry, level untouched. 28 receipt mentions in survival header; schema v20. Farming API: Plant/Harvest/Feed/Collect only.

### Changes
- —

### Next step
None. Waiting for the user to answer Q00. Verdict recorded in CODEBASE.md Findings.

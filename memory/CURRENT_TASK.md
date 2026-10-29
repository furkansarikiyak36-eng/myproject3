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
Goal: Map everything group-a-code-closure adds to the game plan (phase/task), what it gives us, what we build on top.
Asked by the user: "chatgpt kurduğu her şeyin planda yeri var mı, neler ekletebilir, o kodu alıp üstüne ne kurabiliriz"
Started: 2026-10-29 · Last saved: 2026-10-29 · Branch: feature/simple-intent

### Plan
1. Extract added types/enums/functions from group-a headers (git diff, headers only).
2. Map each feature to ROADMAP tasks; mark fits / needs change / no place.
3. Write the mapping into ROADMAP (per task "group-a gives") and CODEBASE.

### Read
- —

### Findings
- Mapping table written to ROADMAP.md section "What group-a-code-closure gives each task". All 19 pieces have a place; only conflict: item-key region gates (keep for climate only). P0, P1 entirely missing in group-a.

### Changes
- —

### Next step
None. User decides Q00.

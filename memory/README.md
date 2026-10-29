# Shared project memory

One memory for every assistant that works on MyProject3: Claude Code, Claude web,
ChatGPT, Antigravity IDE. Read this folder instead of analysing the whole repo.

## At the start of a session

1. Read `STATE.md` (one page: where the project is now).
2. Read the newest files in `sessions/` (what the last sessions did).
3. Check what changed since then: `git log --oneline --since=<date of the newest session>`.
4. Read `ROADMAP.md`: take the next `todo` task and follow its "How to continue" rules.
5. Open `modules/<area>.md` for the area of the task, then only the files it names.
6. Need a reference (how another game does it)? Read `references/`.

Code and git history are always the truth. When memory and code disagree, trust
the code and fix the memory.

## At the end of a session

1. Write `sessions/YYYY-MM-DD_<tool>_<topic>.md` (template below), at most ~30 lines.
2. Update `STATE.md` if the state changed (keep it one page).
3. A lasting decision (a rule, a number, an architecture choice) goes to `DECISIONS.md`.
4. A changed module gets its `modules/<area>.md` updated.
5. Commit with the message `memory: <topic>`.

Web chats cannot write to the repo by themselves: ask them for the session file
at the end and commit it (or paste it to a tool that can).

## Session file template

```markdown
# 2026-10-27 · claude-code · symmetric snaps
Branch: feature/simple-intent · Commits: 7de76ff, eaf6d8d
## Done
- ...
## Found
- ...
## Open
- ...
```

## Rules

- Short. Facts, paths, numbers, decisions; no chat transcripts.
- Write in English (code language); replies to the user stay in Turkish.
- Analyses of reference images and videos go to `references/` as our own text.
  Never put other games' or people's images, models, textures or long quotes in the repo.
- No secrets, keys or personal data.
- Old sessions: when `sessions/` gets long, summarise the oldest into `sessions/ARCHIVE.md`.

## Files

| File | What |
|---|---|
| `STATE.md` | Current state: branches, what works, what is open. |
| `ROADMAP.md` | Ordered tasks of the game plan, honest status, how to continue. |
| `DECISIONS.md` | Lasting decisions and why. |
| `modules/*.md` | Per area: files, rules, open issues. |
| `sessions/*.md` | One log per working session. |
| `references/*.md` | Text analyses of reference games and images. |
| `screenshots/<date>_<topic>/` | Our own in-game screenshots (JPG) with a README per set. |

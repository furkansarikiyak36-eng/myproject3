# MyProject3 — guide for Claude

Unreal Engine 5.8 C++ survival/building game (Sons of the Forest style log building). Reply to the user in Turkish; code, comments and commit messages in English.

## Read this first, then only the files the task needs

Do not scan the whole repo. Find the module in the map below, read its folder `CLAUDE.md` if there is one, and open only those files. Use Grep for a symbol instead of reading big files whole (`LogCarryComponent.cpp` and `LogBuildPiece.cpp` are 3000+ lines).

Skip unless the task is about them: `Content/`, `AssetPackages/`, `Binaries/`, `Intermediate/`, `Saved/`, and the `Docs/Architecture/PASS_*`, `PHASE_*` history documents.

## Module map (`Source/MyProject3/`)

| Area | Files | What it does |
|---|---|---|
| Placement kernel | `Placement/` (see `Placement/CLAUDE.md`) | Every placement: solve, preview, all-or-nothing commit. Profiles in `Config/Placement/PlacementProfiles.json`. |
| Logs | `LogBuildPiece.*`, `LogBuildingTypes.h`, `ConstructionHelpers.*` | A log: mesh family, cuts, snap points (End.A, End.B, Station.00 per 75 cm). |
| Player hands | `LogCarryComponent.*` | Pick up, carry, aim, cut by aim, feeds the kernel and shows its result. |
| Build tools | `LogBuildToolsComponent.*`, `BuildPlanActor.*`, `ConstructionOpeningActor.*` | Floors, stairs, steps, gables, plans, doors, removal span rules. |
| HUD | `LogSnapHUD.*`, `LogSnapCharacter.*` | Draws previews and status; no placement rules. |
| Stone / stick / rope | `StoneBuildingPiece.*`, `StickPrimitivePiece.*`, `RopeLineActor.*`, `MaterialConnectionSubsystem.*` | Other materials; cross-material joints. |
| Structure | `WorldStructuralGraphSubsystem.*`, `StructuralPieceInterface.h` | Support graph: parents, secondary supports, damage, save. |
| Ledger / identity | `WorldItemOwnershipSubsystem.*`, `WorldObjectIdentity*`, `WorldOperationLedgerSubsystem.*` | Who owns a piece (World/Held/Storage/InTransit), lifecycle. |
| Helper NPC | `HelperCompanionActor.*` | Follow, collect, pile, build plans (uses the kernel commit). |
| World / demo | `VerticalSliceCWorldActor.*` (main test level), `VerticalSliceB*`, `HarvestableTreeActor.*`, `LogContainerActor.*` | Level setup, trees, creek, sleds, storage. |
| Save | `WorldSaveCoordinatorSubsystem.*`, `WorldSimulationSaveGame.h` | Saves every subsystem, including the structural graph. |
| Tests | `Tests/MyProject3CoreTests.cpp`, `Tests/PlacementKernelTests.cpp` | Automation tests (68). Add a test with every behaviour change. |

Plans and status: `Docs/Architecture/GLOBAL_SNAP_SYSTEM_PLAN.md` (placement), `Docs/Demo/RAPOR_SOTF_KUTUK_SISTEMI.md` (log mechanics report).

## Build and test

The Unreal Editor must be closed (Live Coding blocks builds). If the build says "Live Coding is active", ask the user to close the editor and wait for "kapattım".

```powershell
& "C:\Program Files\Epic Games\UE_5.8\Engine\Build\BatchFiles\Build.bat" MyProject3Editor Win64 Development "-Project=C:\Users\merts\Downloads\MyProject3\MyProject3.uproject" -WaitMutex 2>&1 | Select-String -Pattern "error|Result:"
& "C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "C:\Users\merts\Downloads\MyProject3\MyProject3.uproject" -ExecCmds="Automation RunTests MyProject3; Quit" -unattended -nullrhi -NoLiveCoding -nosplash "-abslog=<scratchpad>\tests.log"
```

Count `Result={Success}` and list `Result={Fail` / `Error: Expected` lines in the log. IDE diagnostics ("CoreMinimal.h not found") are clangd noise; only the UBT build counts.

## How the user wants work done

- The user describes structures step by step. Implement exactly that structure, end to end, and check the result against their description before saying it is done. Partial migrations that keep the old system underneath are not done; say plainly what is still old.
- Say what is verified (build, tests) and what is not (PIE / in-game).
- Commit on a feature branch after each working step; push only when asked.

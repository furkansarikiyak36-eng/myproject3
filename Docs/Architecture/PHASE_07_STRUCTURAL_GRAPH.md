# Phase 7 — Shared Structural Graph & Damage

**Branch:** `phase-07-structural-graph-sequential`  
**Base:** `phase-06-inventory-crafting-sequential`  
**Engine target:** Unreal Engine 5.8  
**Status:** Initial shared graph foundation only; M059–M070 remain open.

## Scope

M059–M070 belong to the shared structural/damage domain: explicit support dependencies, support-chain checks, structural state, damage/repair and future integration for mixed wood/stone construction and collapse.

## Design boundary

The graph must use explicit stable IDs and authored support edges. Mesh overlap alone never implies structural support. Invalid endpoints, duplicate edges and cycles are rejected. Durability inputs must be finite and positive and health remains clamped to its configured range.

Unsupported means a graph diagnosis only; it does not mean the actor has physically collapsed. Physics load calculations, material-specific strength, deterministic collapse, log/stone/stick adapters, repair costs, save/load, replication, committed events and Unreal Engine validation remain deferred. Registry statuses remain unchanged until evidence exists.
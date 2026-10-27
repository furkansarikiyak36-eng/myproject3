#!/usr/bin/env python3
"""Validate MyProject3 design-time JSON schemas, sample definitions and M001-M119 registry.

This checks design/catalogue files only. It does not compile Unreal C++, load assets,
or verify that candidate gameplay code is merged into the current default branch.
"""
from __future__ import annotations

import json
import sys
from collections import defaultdict
from pathlib import Path
from typing import Any

try:
    from jsonschema import Draft202012Validator
except ImportError:
    print(
        "Missing dependency. Run: python -m pip install -r "
        "Tools/Validation/requirements-catalogue.txt",
        file=sys.stderr,
    )
    raise SystemExit(2)

ROOT = Path(__file__).resolve().parents[2]
ARCH = ROOT / "Docs" / "Architecture"
SCHEMA_DIR = ARCH / "Schemas"
SAMPLE_DIR = ARCH / "Samples"
REGISTRY_PATH = ARCH / "MECHANIC_REGISTRY_119.json"
REGISTRY_SCHEMA_PATH = ARCH / "MECHANIC_REGISTRY_SCHEMA.json"


def load_json(path: Path) -> Any:
    try:
        return json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as exc:
        raise ValueError(f"{path.relative_to(ROOT)}: {exc}") from exc


def schema_name_for_sample(filename: str) -> str | None:
    if filename.startswith("action-"):
        return "gameplay-action-definition"
    if filename.startswith("build-part"):
        return "build-part-definition"
    if filename.startswith("crop-"):
        return "crop-definition"
    if filename.startswith("feedback-event"):
        return "feedback-event-definition"
    if filename.startswith("gameplay-event"):
        return "gameplay-event-definition"
    if filename.startswith("item-"):
        return "item-definition"
    if filename.startswith("logical-asset"):
        return "logical-asset-definition"
    if filename.startswith("material-"):
        return "material-definition"
    if filename.startswith("process-transition"):
        return "process-transition"
    if filename.startswith("recipe-"):
        return "recipe-definition"
    if filename.startswith("region-"):
        return "region-definition"
    if filename.startswith("source-"):
        return "resource-source-definition"
    if filename.startswith("tool-"):
        return "tool-definition"
    if filename.startswith("weather-profile"):
        return "weather-profile"
    return None


def show_errors(label: str, errors: list[str]) -> None:
    for error in errors:
        print(f"FAIL {label}: {error}")


def main() -> int:
    failures = 0
    schema_files = sorted(SCHEMA_DIR.glob("*.schema.json"))
    if not schema_files:
        print(f"FAIL no schemas found under {SCHEMA_DIR}")
        return 1

    schemas: dict[str, dict[str, Any]] = {}
    for path in schema_files:
        try:
            schema = load_json(path)
            Draft202012Validator.check_schema(schema)
            key = path.name.removesuffix(".schema.json")
            if key in schemas:
                raise ValueError(f"duplicate schema key: {key}")
            schemas[key] = schema
        except Exception as exc:  # schema validation exceptions are intentionally surfaced
            failures += 1
            show_errors(path.relative_to(ROOT).as_posix(), [str(exc)])

    if not failures:
        print(f"PASS {len(schemas)} JSON Schema Draft 2020-12 schemas pass meta-schema validation")

    # Validate all 119 mechanic records against the canonical entry schema.
    try:
        registry = load_json(REGISTRY_PATH)
        registry_schema = load_json(REGISTRY_SCHEMA_PATH)
        Draft202012Validator.check_schema(registry_schema)
        mechanics = registry.get("mechanics")
        if not isinstance(mechanics, list):
            raise ValueError("registry must contain a 'mechanics' array")
        validator = Draft202012Validator(registry_schema)
        ids: list[str] = []
        for index, item in enumerate(mechanics):
            item_errors = sorted(
                validator.iter_errors(item),
                key=lambda err: list(err.absolute_path),
            )
            if item_errors:
                failures += 1
                show_errors(
                    f"{REGISTRY_PATH.relative_to(ROOT).as_posix()}[{index}]",
                    [
                        f"{'/'.join(map(str, err.absolute_path)) or '$'}: {err.message}"
                        for err in item_errors
                    ],
                )
            if isinstance(item, dict) and isinstance(item.get("mechanic_id"), str):
                ids.append(item["mechanic_id"])

        expected = [f"M{i:03d}" for i in range(1, 120)]
        if len(mechanics) != 119:
            failures += 1
            print(f"FAIL registry contains {len(mechanics)} records; expected 119")
        if sorted(ids) != expected:
            failures += 1
            missing = sorted(set(expected) - set(ids))
            duplicate = sorted({value for value in ids if ids.count(value) > 1})
            print(f"FAIL mechanic IDs; missing={missing}, duplicate={duplicate}")
        known_ids = set(ids)
        unresolved: list[str] = []
        for item in mechanics:
            if not isinstance(item, dict):
                continue
            for dependency in item.get("dependencies", []):
                if dependency not in known_ids:
                    unresolved.append(f"{item.get('mechanic_id')} -> {dependency}")
        if unresolved:
            failures += 1
            show_errors("mechanic dependency graph", unresolved)
        if not failures:
            print("PASS M001-M119 registry: 119 unique IDs, entry schema valid, dependencies resolve")
    except Exception as exc:
        failures += 1
        show_errors("mechanic registry", [str(exc)])

    # Validate representative sample records against the matching schema.
    sample_files = sorted(SAMPLE_DIR.glob("*.json"))
    if not sample_files:
        failures += 1
        print(f"FAIL no sample files found under {SAMPLE_DIR}")
    sample_ids: dict[str, list[tuple[str, str]]] = defaultdict(list)
    validated_samples = 0
    for path in sample_files:
        key = schema_name_for_sample(path.name)
        label = path.relative_to(ROOT).as_posix()
        if key is None or key not in schemas:
            failures += 1
            print(f"FAIL {label}: no schema mapping for sample")
            continue
        try:
            sample = load_json(path)
            validator = Draft202012Validator(schemas[key])
            errors = sorted(validator.iter_errors(sample), key=lambda err: list(err.absolute_path))
            if errors:
                failures += 1
                show_errors(
                    label,
                    [
                        f"{'/'.join(map(str, err.absolute_path)) or '$'}: {err.message}"
                        for err in errors
                    ],
                )
                continue
            validated_samples += 1
            definition_id = sample.get("id") if isinstance(sample, dict) else None
            if isinstance(definition_id, str):
                sample_ids[key].append((definition_id, label))
        except Exception as exc:
            failures += 1
            show_errors(label, [str(exc)])

    duplicate_ids: list[str] = []
    for schema_key, entries in sample_ids.items():
        grouped: dict[str, list[str]] = defaultdict(list)
        for definition_id, label in entries:
            grouped[definition_id].append(label)
        for definition_id, paths in grouped.items():
            if len(paths) > 1:
                duplicate_ids.append(f"{schema_key}: {definition_id} in {', '.join(paths)}")
    if duplicate_ids:
        failures += 1
        show_errors("sample definition ID uniqueness", duplicate_ids)

    if validated_samples == len(sample_files) and not duplicate_ids:
        print(
            f"PASS {validated_samples} sample definitions validate; "
            "sample IDs are unique within each schema family"
        )
    print(
        "INFO Sample files are representative examples, not the complete data catalogue; "
        "cross-references to definitions absent from the sample set are not treated as errors."
    )
    if failures:
        print(f"FAILED: {failures} issue(s) found")
        return 1
    print("SUCCESS: design-time catalogue validation passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

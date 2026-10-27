"""Copies Config/Placement/PlacementProfiles.json into the built-in catalogue.

Packaged builds may not ship the Config/Placement folder, so the game keeps a
compiled copy (FPlacementProfileRegistry::GetBuiltInCatalogue). Run this after
editing the JSON; the automation test Placement.BuiltInCatalogueMatchesProjectFile
fails while the two differ.

    python UnrealEditorScripts/SyncPlacementCatalogue.py
"""
import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
CATALOGUE = ROOT / "Config" / "Placement" / "PlacementProfiles.json"
SOURCE = ROOT / "Source" / "MyProject3" / "Placement" / "PlacementProfiles.cpp"

BEGIN = "    // BEGIN GENERATED CATALOGUE (UnrealEditorScripts/SyncPlacementCatalogue.py)\n"
END = "    // END GENERATED CATALOGUE\n"


def main() -> int:
    text = CATALOGUE.read_text(encoding="utf-8").strip()
    if ")JSON\"" in text:
        print("The catalogue contains the raw string terminator )JSON\"", file=sys.stderr)
        return 1
    # MSVC limits one string literal to about 16 KB, so the text goes in pieces.
    pieces = []
    while text:
        pieces.append(text[:8000])
        text = text[8000:]
    body = "".join(f'        TEXT(R"JSON({piece})JSON")\n' for piece in pieces)
    block = f"{BEGIN}    static const TCHAR* Catalogue =\n{body}        ;\n    return Catalogue;\n{END}"

    source = SOURCE.read_text(encoding="utf-8")
    pattern = re.compile(re.escape(BEGIN) + ".*?" + re.escape(END), re.S)
    if not pattern.search(source):
        print("Generated block markers not found in PlacementProfiles.cpp", file=sys.stderr)
        return 1
    SOURCE.write_text(pattern.sub(lambda _: block, source), encoding="utf-8")
    print(f"Built-in catalogue updated from {CATALOGUE.name} ({len(pieces)} piece(s)).")
    return 0


if __name__ == "__main__":
    sys.exit(main())

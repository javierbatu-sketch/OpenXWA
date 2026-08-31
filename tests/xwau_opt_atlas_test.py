from pathlib import Path
import re
import sys

source = Path("src/xwa_remaster/opt_mesh.c").read_text(encoding="utf-8")

call = re.search(
    r"Aeron_OptModelBuildMemory\s*\("
    r".*?"
    r"&\(AeronOptModelBuildOptions\)\s*\{"
    r"(.*?)"
    r"\}\s*,\s*out\s*,\s*&build_error\s*\)",
    source,
    re.DOTALL,
)

if not call:
    print("FAIL: no se encontro el initializer de AeronOptModelBuildOptions")
    sys.exit(1)

options = call.group(1)

if not re.search(r"\.max_atlas_size\s*=\s*8192\b", options):
    print(
        "FAIL: XwaRemasterOptMesh_Build no solicita "
        "max_atlas_size = 8192"
    )
    sys.exit(1)

print("PASS: OpenXWA solicita atlas OPT de 8192")

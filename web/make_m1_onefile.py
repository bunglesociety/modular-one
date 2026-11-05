#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Alexis Arakis (Bungle Society)
"""Modular One's installer as ONE file to open from the disk in Chrome (file://, nothing sent anywhere): install.html
with fm1pkg.js, fm1ota.js and the package inside.   python3 web/make_m1_onefile.py PACKAGE.fwsc VERSION OUT.html"""
import base64
import json
import re
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
from make_site import product_of, strip_module   # noqa: E402


def main(pkg, version, out):
    raw = Path(pkg).read_bytes()
    product = product_of(raw)
    if not re.fullmatch(r"FM-1_9\d\d", product) or b"FELUCCA-LOADER-1" not in raw:
        raise SystemExit(f"{pkg}: not a Modular One package ({product!r})")
    html = (HERE / "install.html").read_text(encoding="utf-8")
    lib = strip_module((HERE / "fm1pkg.js").read_text(encoding="utf-8")) + "\n" + \
        strip_module((HERE / "fm1ota.js").read_text(encoding="utf-8")) + "\n" + \
        strip_module((HERE / "fm1probe.js").read_text(encoding="utf-8"))
    meta = json.dumps({"version": version, "product": product, "family": "one", "name": "Modular One", "b64": base64.b64encode(raw).decode()})
    for mark, val in (("/*LIB*/", lib), ("/*META*/", meta)):
        if html.count(mark) != 1:
            raise SystemExit(f"install.html must contain {mark} once")
        html = html.replace(mark, val)
    Path(out).write_text(html, encoding="utf-8")
    print(f"installer: {out} ({len(html)} B, {product}, {version})")


if __name__ == "__main__":
    if len(sys.argv) != 4:
        sys.exit(__doc__)
    main(*sys.argv[1:])

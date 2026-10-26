#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Alexis Arakis (Bungle Society)
"""Make the Modular One site (GitHub Pages, or any https host):

  index.html                       home.html: the Modular One page (img/: the screens, the town's day)
  install/index.html               install.html with fm1pkg.js, fm1ota.js and the metadata inlined
  carnet/index.html                carnet.html: songs and sketches, MIDI in and out
  firmware/modular-one-VER.fwsc    the package
  .nojekyll                        (GitHub Pages: serve the files as they are)

  python3 web/make_m1_site.py build/felucca.fwsc 0.2-beta OUT_DIR

Try it before publishing:  cd OUT_DIR && python3 -m http.server 8000  ->  http://localhost:8000
(Web MIDI needs https or localhost.)
"""
import json
import re
import shutil
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
from make_site import product_of, strip_module   # noqa: E402  (Felucca's site maker: the same package checks)


def main(pkg, version, out):
    pkg, out = Path(pkg), Path(out)
    raw = pkg.read_bytes()
    product = product_of(raw)
    if not re.fullmatch(r"FM-1_9\d\d", product):
        raise SystemExit(f"{pkg}: identity {product!r} is not a Felucca package (FM-1_9xx)")
    if b"FELUCCA-LOADER-1" not in raw:
        raise SystemExit(f"{pkg}: no update loader in it")
    name = f"modular-one-{re.sub(r'[^A-Za-z0-9.-]', '-', version)}.fwsc"
    html = (HERE / "install.html").read_text(encoding="utf-8")
    lib = strip_module((HERE / "fm1pkg.js").read_text(encoding="utf-8")) + "\n" + \
        strip_module((HERE / "fm1ota.js").read_text(encoding="utf-8"))
    meta = json.dumps({"version": version, "product": product, "pkg": "../firmware/" + name})
    for mark in ("/*LIB*/", "/*META*/"):
        if html.count(mark) != 1:
            raise SystemExit(f"install.html must contain {mark} once")
    html = html.replace("/*LIB*/", lib).replace("/*META*/", meta)
    inst, car, fw = out / "install", out / "carnet", out / "firmware"
    for d in (inst, car, fw):
        d.mkdir(parents=True, exist_ok=True)
    for old in fw.glob("*.fwsc"):                    # one package: the current one
        if old.name != name:
            old.unlink()
    shutil.copy(pkg, fw / name)
    (inst / "index.html").write_text(html, encoding="utf-8")
    shutil.copy(HERE / "carnet.html", car / "index.html")
    shutil.copy(HERE / "home.html", out / "index.html")
    (out / "img").mkdir(exist_ok=True)
    for f in sorted((HERE / "img").glob("*")):
        shutil.copy(f, out / "img" / f.name)
    (out / ".nojekyll").write_text("", encoding="utf-8")
    print(f"site: {out}: index.html, install/ ({len(html)} B), carnet/, firmware/{name} ({len(raw)} B, {product})")


if __name__ == "__main__":
    if len(sys.argv) != 4:
        sys.exit(__doc__)
    main(*sys.argv[1:4])

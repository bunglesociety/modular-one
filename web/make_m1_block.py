#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Alexis Arakis (Bungle Society)
"""The Modular One page for bunglesociety.com: a Squarespace code block (HTML + CSS only, no script: the Basic plan),
made from home.html (its texts, FR and EN, and its layout). FR / EN: two radio buttons, CSS only. The pictures are
the GitHub Pages site's (bunglesociety.github.io/modular-one/img/).

  python3 web/make_m1_block.py web/block.html
"""
import html as H
import json
import re
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
SITE = "https://bunglesociety.github.io/modular-one/"


def texts():
    src = (HERE / "home.html").read_text(encoding="utf-8")
    js = src[src.index("const TEXT = {") + 13:src.index("};", src.index("const TEXT = {")) + 1]
    js = re.sub(r"^(\s*)(fr|en):", r'\1"\2":', js, flags=re.M)
    js = re.sub(r'(^|,\s*|\{\s*)([A-Za-z0-9]+):\s', r'\1"\2": ', js, flags=re.M)
    js = re.sub(r",(\s*[}\]])", r"\1", js)
    return json.loads(js)


def main(out):
    T = texts()

    def t(k):
        return f'<span class="fr">{H.escape(T["fr"][k], quote=False)}</span><span class="en">{H.escape(T["en"][k], quote=False)}</span>'

    feats = "".join(
        f'      <div class="feat"><img src="{SITE}img/{img}" alt="" width="240" height="240" loading="lazy"><div><h3 class="px">{t(f"f{i}t")}</h3><p>{t(f"f{i}")}</p></div></div>\n'
        for i, img in enumerate(("presets.png", "home.png", "magneto.png", "liste.png", "fm.png", "pistes.png"), 1))
    faq = "".join(f'    <details><summary>{t(f"q{i}")}</summary><p>{t(f"a{i}")}</p></details>\n' for i in range(1, 10) if f"q{i}" in T["fr"])
    style = (HERE / "block.css").read_text(encoding="utf-8")
    body = f'''<div class="m1w">
  <input type="radio" name="m1w-lang" id="m1w-fr" class="m1w-l" checked>
  <input type="radio" name="m1w-lang" id="m1w-en" class="m1w-l">
  <div class="lang"><label for="m1w-fr">FR</label>/<label for="m1w-en">EN</label></div>

  <div class="hero">
    <div>
      <h1 class="px">Modular<br>One</h1>
      <p class="lead">{t("lead")}</p>
      <p class="note" style="margin:0 0 22px">{t("by")} · <a href="https://www.youtube.com/@bunglesociety" target="_blank" rel="noopener">YouTube</a></p>
      <div class="cta">
        <a class="btn" href="{SITE}install/" target="_blank" rel="noopener">{t("install")}</a>
        <a class="btn ghost" href="{SITE}carnet/" target="_blank" rel="noopener">{t("carnet")}</a>
        <a class="btn ghost" href="https://github.com/bunglesociety/modular-one" target="_blank" rel="noopener">GitHub</a>
      </div>
      <p class="note">{t("free")}</p>
    </div>
    <div class="screen">
      <img src="{SITE}img/ville.gif" alt="Modular Town" width="240" height="240">
      <div class="lbl"><span><span class="led"></span>M-VAVE FM-1</span><span>MODULAR TOWN</span></div>
    </div>
  </div>

  <div class="family">
    <span class="grow"><b><span class="fr">La famille Modular :</span><span class="en">The Modular family:</span></b> <span class="fr">Modular Town, le synthé-ville pour ton ordinateur (un patch gratuit pour Amorph, le plugin d'Artists in DSP). Modular One en est le petit frère sur le FM-1. Modular Koi fait du FM-1 un tambour à languettes. Modular Compagnon vit sur le clavier SMK-37.</span><span class="en">Modular Town, the town-synth for your computer (a free patch for Amorph, the plugin by Artists in DSP). Modular One is its little brother on the FM-1. Modular Koi turns the FM-1 into a steel tongue drum. Modular Compagnon lives on the SMK-37 keyboard.</span></span>
    <a href="https://amorph.audio/patches/modular-town-v3/" target="_blank" rel="noopener">Modular Town →</a>
    <a href="https://bunglesociety.github.io/modular-koi/" target="_blank" rel="noopener">Modular Koi →</a>
    <a href="https://bunglesociety.github.io/modular-compagnon/" target="_blank" rel="noopener">Modular Compagnon →</a>
  </div>

  <section>
    <h2 class="px">{t("storyTitle")}</h2>
    <div class="story"><p>{t("story1")}</p><p>{t("story2")}</p></div>
  </section>

  <section>
    <h2 class="px">{t("videoTitle")}</h2>
    <div class="video"><iframe src="https://www.youtube-nocookie.com/embed/uFcVOXPicuw" title="Modular One" loading="lazy"
      allow="accelerometer; encrypted-media; gyroscope; picture-in-picture; fullscreen" referrerpolicy="strict-origin-when-cross-origin" allowfullscreen></iframe></div>
  </section>

  <section>
    <h2 class="px">{t("featTitle")}</h2>
    <div class="feats">
{feats}    </div>
  </section>

  <section>
    <h2 class="px">{t("howTitle")}</h2>
    <ol class="steps"><li>{t("h1")}</li><li>{t("h2")}</li><li>{t("h3")}</li></ol>
    <div class="after"><a class="btn" href="{SITE}install/" target="_blank" rel="noopener">{t("install")}</a><span class="note" style="margin:0">{t("howNote")} <span class="fr">La page refuse un appareil qui n'est pas un FM-1 : rien n'est écrit dessus.</span><span class="en">The page refuses a device that is not an FM-1: nothing is written to it.</span></span></div>
  </section>

  <section class="faq">
    <h2 class="px">{T["fr"]["faqTitle"]}</h2>
{faq}  </section>

  <p class="foot">{t("based")} <a href="https://github.com/hugelton/Felucca" target="_blank" rel="noopener">Felucca</a> by Leo Kuroshita (@kurogedelic), <a href="https://hugelton.com" target="_blank" rel="noopener">Hügelton Instruments</a>. {t("thanks")}<br>
    {t("license")} <a href="https://github.com/bunglesociety/modular-one" target="_blank" rel="noopener">github.com/bunglesociety/modular-one</a>. {t("tm")}</p>
</div>
'''
    Path(out).write_text("<style>\n" + style + "</style>\n\n" + body, encoding="utf-8")
    print(f"{out}: {len(style) + len(body)} chars")


if __name__ == "__main__":
    if len(sys.argv) != 2:
        sys.exit(__doc__)
    main(sys.argv[1])

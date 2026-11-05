// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Alexis Arakis (Bungle Society)
// What runs on the connected FM-1 or SMK-37, read before installing: the update identity (MODEL_NNN, the handshake of
// fm1ota.js) and the editor's INFO (editor.c: "FELUCCA " + the version string). From both: the firmware, its version,
// and whether this package may go on it. Used by the install pages of Modular One, Modular Koi and Modular Compagnon
// (inlined after fm1ota.js: it uses its Updater, sameModel and IS_OTA-like tests).
//
// The names the firmwares give (INFO):
//   Modular Koi        "FELUCCA KOI 0.1"                 (tools/build.py --board koi --release 0.1)
//   Modular One        "FELUCCA M1 0.3 BETA"             (0.3 on; before: "FELUCCA M1 0.1 BETA", "FELUCCA 0.2-BETA")
//   Modular Compagnon  "FELUCCA MC 1.1"                  (1.1 on; 1.0: "FELUCCA v1.0", identity _900)
//   Felucca            "FELUCCA v1.6.1"
//   M-VAVE's own firmware gives an identity (MODEL_0NN) and no INFO.

const ED_HDR = [0x7D, 0x46, 0x4C];

// the editor's INFO on an open link: its version string, or null (no answer: not a Felucca firmware)
export async function readInfo(link, ms = 900) {
  link.drain();
  if (!link.send([0xF0, ...ED_HDR, 1, 0xF7])) return null;
  const end = Date.now() + ms;
  while (Date.now() < end) {
    const p = await link.read(end - Date.now());
    if (!p) break;
    if (p[1] === ED_HDR[0] && p[2] === ED_HDR[1] && p[3] === ED_HDR[2] && p[4] === 1) {
      let s = "";
      for (let i = 5; i < p.length - 1 && p[i]; i++) s += String.fromCharCode(p[i]);
      return s;
    }
  }
  return null;
}

// "0.3", "1.6.1" -> [0, 3], [1, 6, 1]; compare: -1 0 1
export const verParts = (v) => String(v || "").replace(/^v/i, "").split(/[^0-9]+/).filter((x) => x !== "").map(Number);
export function verCmp(a, b) {
  const x = verParts(a), y = verParts(b);
  for (let i = 0; i < Math.max(x.length, y.length); i++) {
    const d = (x[i] || 0) - (y[i] || 0);
    if (d) return d < 0 ? -1 : 1;
  }
  return 0;
}

// identity { text, model, version } (+ the INFO string) -> { family, name, version }
//   family: "koi" | "one" | "compagnon" | "felucca" | "stock" | "ota" | "unknown"
export function classify(id, info) {
  if (/^ota-/i.test(id.model)) return { family: "ota", name: "", version: "" };
  const nnn = id.version, idver = nnn >= 900 ? `${Math.floor(nnn / 10) % 10}.${nnn % 10}` : "";
  if (info == null)
    return nnn < 900 ? { family: "stock", name: "M-VAVE", version: String(nnn).padStart(3, "0") }
                     : { family: "unknown", name: "", version: idver };
  const s = info.replace(/^FELUCCA\s*/i, "").trim();
  let m;
  if ((m = /^KOI\s+v?([0-9][0-9.]*)/i.exec(s))) return { family: "koi", name: "Modular Koi", version: m[1] };
  if ((m = /^MC\s+v?([0-9][0-9.]*)/i.exec(s))) return { family: "compagnon", name: "Modular Compagnon", version: m[1] };
  if ((m = /^M1\s+v?([0-9][0-9.]*)/i.exec(s))) return { family: "one", name: "Modular One", version: m[1] };
  if (/^FM-1$/i.test(id.model) && (m = /^([0-9]\.[0-9])-?\s*BETA/i.exec(s)))   // Modular One 0.1 / 0.2 releases
    return { family: "one", name: "Modular One", version: m[1] };
  if (!/^FM-1$/i.test(id.model) && nnn >= 900 && (m = /^v?([0-9]\.[0-9][0-9.]*)/i.exec(s)) &&
      (m[1].startsWith(idver) || /^1\.0(\.|$)/.test(m[1])))
    return { family: "compagnon", name: "Modular Compagnon", version: m[1] };   // Modular Compagnon 1.0 on an SMK-37 (_900)
  if ((m = /^v?([0-9][0-9.]*)/i.exec(s))) return { family: "felucca", name: "Felucca", version: m[1] };
  return { family: "unknown", name: s, version: idver };
}

// what is on the device: null (none found) or { id, info, family, name, version, link: closed }
export async function probe(updater) {
  const dev = await updater.find(() => true);
  if (!dev) return null;
  const ota = /^ota-/i.test(dev.id.model);
  const info = ota ? null : await readInfo(dev.link);
  dev.link.close();
  return { id: dev.id, info, ...classify(dev.id, info) };
}

// may this package go on it? pkg: { family, version, product }. -> { ok, key, warn }
//   key: "otherModel" | "ota" | "same" | "update" | "older" | "replace" | "fresh"
export function verdict(dev, pkg) {
  const model = pkg.product.split("_")[0];
  if (dev.id.model.replace(/^ota-/i, "") !== model) return { ok: false, key: "otherModel" };
  if (dev.family === "ota") return { ok: true, key: "ota" };
  if (dev.family === pkg.family) {
    const c = verCmp(dev.version, pkg.version);
    if (c === 0) return { ok: true, key: "same" };
    return c < 0 ? { ok: true, key: "update" } : { ok: true, key: "older", warn: true };
  }
  return { ok: true, key: dev.family === "stock" ? "fresh" : "replace" };
}

// what the device runs, in words (the pages add "on your FM-1:")
export function devLabel(dev, lang) {
  const fr = lang === "fr";
  switch (dev.family) {
  case "stock": return (fr ? "le firmware d'origine M-VAVE (" : "M-VAVE's original firmware (") + dev.version + ")";
  case "ota": return fr ? "en mode mise à jour" : "in update mode";
  case "unknown": return fr ? "un firmware inconnu" + (dev.version ? " (" + dev.version + ")" : "") : "an unknown firmware" + (dev.version ? " (" + dev.version + ")" : "");
  default: return dev.name + " " + dev.version;
  }
}

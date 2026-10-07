// Writes LEVEL in web/page/kits.js: for every factory kit, the LEVEL knob (0 to 1) that puts one full-accent hit
// of each voice at its TARGET peak on the main, measured with build/shogun.wasm. Run after the wasm build.
import { readFileSync, writeFileSync } from "node:fs";

const root = new URL("..", import.meta.url).pathname;
const file = root + "web/page/kits.js";
const src = readFileSync(file, "utf8");
const { KIT, STYLE, TARGET, kitOf } = new Function(src.replace(/const LEVEL=.*$/m, "") + "\nreturn {KIT,STYLE,TARGET,kitOf};")();
const mod = new WebAssembly.Module(readFileSync(root + "build/shogun.wasm"));
const VI = ["BD1","BD2","SD","RS","CY","OH","HH","CL","CP","LTC","MTC","HTC","CB","MA","LEAD","BASS"];

function peak(kit, v) {
  const x = new WebAssembly.Instance(mod, { env: { sin: Math.sin, cos: Math.cos, exp: Math.exp, pow: Math.pow, tanh: Math.tanh } }).exports;
  x.sg_init();
  for (let i = 0; i < x.sg_knob_count(); i++) {
    const m = new Uint8Array(x.memory.buffer); let p = x.sg_knob_name(i), s = "";
    while (m[p]) s += String.fromCharCode(m[p++]);
    if (kit[s] != null) x.sg_set_knob(i, kit[s]);
  }
  if (v >= 14) x.sg_trigger_note(v, v == 14 ? 60 : 36, 1); else x.sg_trigger(v, 1, 0);
  let pk = 0;
  for (let o = 0; o < 24000; o += 1024) {
    x.sg_process(1024);
    const L = new Float32Array(x.memory.buffer, x.sg_out_l(), 1024), R = new Float32Array(x.memory.buffer, x.sg_out_r(), 1024);
    for (let i = 0; i < 1024; i++) pk = Math.max(pk, Math.abs(L[i]), Math.abs(R[i]));
  }
  return pk;
}

const out = {};
for (const name of Object.keys(STYLE)) {
  const kit = kitOf(name), lv = {};
  VI.forEach((k, v) => { const p = peak(kit, v); lv[k] = +Math.min(1, TARGET[k] / p).toFixed(3); });
  out[name] = lv;
}
writeFileSync(file, src.replace(/const LEVEL=.*$/m, "const LEVEL=/*__LEVEL__*/" + JSON.stringify(out) + ";"));
console.log("levels for", Object.keys(out).length, "kits");
for (const [n, lv] of Object.entries(out)) console.log(n.padEnd(11), VI.map((k) => k + " " + lv[k]).join("  "));

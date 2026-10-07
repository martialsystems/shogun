// Runs web/parity_scenario.txt through build/shogun.wasm and compares every sample with the native
// build of the same entry points (build/web_parity). Usage: node web/test_wasm.mjs
import { readFileSync } from "node:fs";
import { execFileSync } from "node:child_process";

const root = new URL("..", import.meta.url).pathname;
const scenario = readFileSync(root + "web/parity_scenario.txt", "utf8");
const native = execFileSync(root + "build/web_parity", [root + "web/parity_scenario.txt"], { maxBuffer: 1 << 28 })
  .toString().trim().split("\n").map((l) => l.split(" ").map(Number));

const { instance } = await WebAssembly.instantiate(readFileSync(root + "build/shogun.wasm"), {
  env: { sin: Math.sin, exp: Math.exp, pow: Math.pow, tanh: Math.tanh },
});
const x = instance.exports;
const str = (p) => { const m = new Uint8Array(x.memory.buffer); let s = ""; while (m[p]) s += String.fromCharCode(m[p++]); return s; };
x.sg_init();
const knobs = {};
for (let i = 0; i < x.sg_knob_count(); i++) knobs[str(x.sg_knob_name(i))] = i;

const out = [];
for (const line of scenario.split("\n")) {
  const w = line.trim().split(/\s+/);
  if (!w[0] || w[0].startsWith("#")) continue;
  if (w[0] === "knob") x.sg_set_knob(knobs[w[1]], +w[2]);
  else if (w[0] === "call") x[w[1]](...w.slice(2).map(Number));
  else if (w[0] === "process") {
    for (let n = +w[1]; n > 0; n -= 1024) {
      const k = Math.min(n, 1024);
      x.sg_process(k);
      const L = new Float32Array(x.memory.buffer, x.sg_out_l(), k), R = new Float32Array(x.memory.buffer, x.sg_out_r(), k);
      for (let i = 0; i < k; i++) out.push([L[i], R[i]]);
    }
  }
}

let worst = 0, at = -1, loud = 0;
for (let i = 0; i < out.length; i++) {
  const d = Math.max(Math.abs(out[i][0] - native[i][0]), Math.abs(out[i][1] - native[i][1]));
  if (d > worst) { worst = d; at = i; }
  loud = Math.max(loud, Math.abs(native[i][0]), Math.abs(native[i][1]));
}
const ok = out.length === native.length && worst < 1e-5 && loud > 0.1;
console.log(`${out.length} samples, peak ${loud.toFixed(3)}, largest wasm/native difference ${worst.toExponential(2)} at ${at}`);
console.log(ok ? "wasm matches the native engine" : "MISMATCH");
process.exit(ok ? 0 : 1);

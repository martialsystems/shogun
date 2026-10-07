// Runs web/parity_scenario.txt through build/shogun.wasm and compares every sample with the native
// build of the same entry points (build/web_parity). Usage: node web/test_wasm.mjs
import { readFileSync } from "node:fs";
import { execFileSync } from "node:child_process";

const root = new URL("..", import.meta.url).pathname;
const scenario = readFileSync(root + "web/parity_scenario.txt", "utf8");
const native = execFileSync(root + "build/web_parity", [root + "web/parity_scenario.txt"], { maxBuffer: 1 << 28 })
  .toString().trim().split("\n").map((l) => l.split(" ").map(Number));

const { instance } = await WebAssembly.instantiate(readFileSync(root + "build/shogun.wasm"), {
  env: { sin: Math.sin, cos: Math.cos, exp: Math.exp, pow: Math.pow, tanh: Math.tanh, log: Math.log },
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

// The bay law, on a fresh instance with an empty pattern: a cable from CLK OUT (or ACC OUT) into BD1 Trig fires BD1
// only with the switch on EXT, and the cable never moves the switch. ACC OUT is high only on a loud step.
async function bayPeak(mode, source, loudStep) {
  const { instance: i2 } = await WebAssembly.instantiate(readFileSync(root + "build/shogun.wasm"), {
    env: { sin: Math.sin, cos: Math.cos, exp: Math.exp, pow: Math.pow, tanh: Math.tanh, log: Math.log },
  });
  const y = i2.exports;
  y.sg_init();
  y.sg_set_level(0, 1);
  y.sg_set_master(1);
  y.sg_set_tempo(120);
  y.sg_set_mode(mode);
  // the BD2 track carries the accent: one step of 16, at level 0 so it is never heard
  y.sg_set_level(1, 0);
  y.sg_set_track(1, 16, 0, 0, 0);
  if (loudStep >= 0) y.sg_set_drum(1, loudStep, 1, 2, -1, -1);
  y.sg_commit();
  y.sg_patch(0, source);
  y.sg_set_running(1);
  let peak = 0;
  for (let n = 0; n < 48000; n += 1024) {
    y.sg_process(1024);
    const L = new Float32Array(y.memory.buffer, y.sg_out_l(), 1024);
    for (const v of L) peak = Math.max(peak, Math.abs(v));
  }
  return peak;
}
const law = [
  ["CLK OUT into BD1 Trig, INT: silent", (await bayPeak(0, 1, -1)) < 1e-6],
  ["CLK OUT into BD1 Trig, EXT: fires", (await bayPeak(1, 1, -1)) > 0.05],
  ["ACC OUT into BD1 Trig, EXT, no loud step: silent", (await bayPeak(1, 2, -1)) < 1e-6],
  ["ACC OUT into BD1 Trig, EXT, one loud step: fires", (await bayPeak(1, 2, 0)) > 0.05],
  ["ACC OUT into BD1 Trig, INT, one loud step: silent", (await bayPeak(0, 2, 0)) < 1e-6],
];
// The LFO: tempo-synced rate, 0 to 5 V around 2.5 V scaled by amount, phase 0 on transport start, and a CV input that
// moves the voice it feeds.
async function fresh() {
  const { instance: i3 } = await WebAssembly.instantiate(readFileSync(root + "build/shogun.wasm"), {
    env: { sin: Math.sin, cos: Math.cos, exp: Math.exp, pow: Math.pow, tanh: Math.tanh, log: Math.log },
  });
  const y = i3.exports;
  y.sg_init();
  return y;
}
async function lfoRun(cpb, shape, amount, n, start) {
  const y = await fresh();
  y.sg_set_tempo(120);
  y.sg_set_lfo(cpb, 0, shape, amount);
  if (start) y.sg_set_running(1);
  const v = [];
  for (let i = 0; i < n; i++) { y.sg_process(1); v.push(y.sg_lfo_volts()); }
  return v;
}
{
  const saw = await lfoRun(4, 2, 1, 48000, true);   // 1/16 at 120 BPM
  let wraps = 0;
  for (let i = 1; i < saw.length; i++) if (saw[i] < saw[i - 1] - 2) wraps++;
  law.push(["LFO 1/16 at 120 BPM is 8 Hz (" + wraps + " wraps between 8 cycles in 1 s)", wraps === 7]);
  const sine = await lfoRun(4, 0, 1, 6000, true);
  const lo = Math.min(...sine), hi = Math.max(...sine);
  law.push(["LFO sine at amount 1 spans 0 to 5 V around 2.5 V", lo > -1e-6 && lo < 0.01 && hi > 4.99 && hi < 5 + 1e-6]);
  law.push(["LFO starts at phase 0 on transport start", Math.abs(sine[0] - 2.5) < 1e-9]);
  const zero = await lfoRun(4, 0, 0, 6000, true);
  law.push(["LFO amount 0 is 0 V", zero.every((x) => x === 0)]);
  const sh = await lfoRun(4, 4, 1, 12000, true);
  const changes = [];
  for (let i = 1; i < sh.length; i++) if (sh[i] !== sh[i - 1]) changes.push(i);
  law.push(["LFO sample and hold changes once per cycle (" + changes.join(", ") + ")", changes.length === 1 && Math.abs(changes[0] - 6000) <= 1]);
}
async function bd1With(patch) {
  const y = await fresh();
  y.sg_set_level(0, 1);
  y.sg_set_master(1);
  y.sg_set_lfo(0.25, 0, 3, 1);    // 1/1 square: 5 V for the first bar half
  if (patch) y.sg_patch(19, 4);   // LFO OUT into BD1 PITCH
  y.sg_trigger(0, 1, 0);
  const out = [];
  for (let n = 0; n < 4096; n += 1024) { y.sg_process(1024); out.push(...new Float32Array(y.memory.buffer, y.sg_out_l(), 1024)); }
  return out;
}
{
  const a = await bd1With(false), b = await bd1With(true);
  law.push(["LFO OUT into BD1 PITCH changes the kick", a.some((x, i) => Math.abs(x - b[i]) > 1e-3)]);
}
for (const [name, pass] of law) console.log((pass ? "ok   " : "FAIL ") + name);
process.exit(ok && law.every((l) => l[1]) ? 0 : 1);

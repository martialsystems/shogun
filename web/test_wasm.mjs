// Runs web/parity_scenario.txt through build/shogun.wasm and compares every sample with the native
// build of the same entry points (build/web_parity). Usage: node web/test_wasm.mjs
import { readFileSync } from "node:fs";
import { execFileSync } from "node:child_process";

const root = new URL("..", import.meta.url).pathname;
// The engine's transcendental math comes from the page (Math.*): the same imports as web/page/host.js.
const ENV = { env: { sin: Math.sin, cos: Math.cos, tan: Math.tan, exp: Math.exp, exp2: (x) => Math.pow(2, x), pow: Math.pow, tanh: Math.tanh, log: Math.log, log2: Math.log2, log10: Math.log10, log1p: Math.log1p, atan2: Math.atan2 } };
const scenario = readFileSync(root + "web/parity_scenario.txt", "utf8");
const native = execFileSync(root + "build/web_parity", [root + "web/parity_scenario.txt"], { maxBuffer: 1 << 28 })
  .toString().trim().split("\n").map((l) => l.split(" ").map(Number));

const { instance } = await WebAssembly.instantiate(readFileSync(root + "build/shogun.wasm"), ENV);
const x = instance.exports;
const str = (p) => { const m = new Uint8Array(x.memory.buffer); let s = ""; while (m[p]) s += String.fromCharCode(m[p++]); return s; };
x.sg_init(48000);
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
  const { instance: i2 } = await WebAssembly.instantiate(readFileSync(root + "build/shogun.wasm"), ENV);
  const y = i2.exports;
  y.sg_init(48000);
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
  const { instance: i3 } = await WebAssembly.instantiate(readFileSync(root + "build/shogun.wasm"), ENV);
  const y = i3.exports;
  y.sg_init(48000);
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
  // v2 LFO (§8.2, §8.3): LFO 1, synced 1/16, unipolar; declicked (τ ≥ 0.25 ms), so edges are smooth, not steps.
  const sine = await lfoRun(4, 0, 1, 48000, true);  // 1/16 at 120 BPM
  const ups = [];
  for (let i = 1; i < sine.length; i++) if (sine[i - 1] < 2.5 && sine[i] >= 2.5) ups.push(i);
  const period = (ups[ups.length - 1] - ups[0]) / (ups.length - 1);
  law.push(["LFO 1/16 at 120 BPM is 8 Hz (period " + period.toFixed(2) + " samples)", Math.abs(period - 6000) < 1]);
  const lo = Math.min(...sine.slice(0, 6000)), hi = Math.max(...sine.slice(0, 6000));
  law.push(["LFO sine at amount 1 spans 0 to 5 V around 2.5 V (" + lo.toFixed(4) + " .. " + hi.toFixed(4) + ")", lo > -1e-6 && lo < 0.01 && hi > 4.99 && hi < 5 + 1e-6]);
  law.push(["LFO starts at phase 0 on transport start (" + sine[0].toFixed(4) + " V)", Math.abs(sine[0] - 2.5) < 0.01]);
  const zero = await lfoRun(4, 0, 0, 6000, true);
  law.push(["LFO amount 0 is 0 V", zero.every((x) => Math.abs(x) < 1e-12)]);
  const sh = await lfoRun(4, 4, 1, 18000, true);
  let stray = 0;
  for (let i = 1; i < sh.length; i++) if (sh[i] !== sh[i - 1] && (i % 6000) > 240) stray++;
  law.push(["LFO sample and hold moves only in the 5 ms after each cycle start (" + stray + " stray changes)", stray === 0]);
}
{
  // v2 facade at a 44.1 kHz AudioContext: coefficients at that rate, latency 23, 454 params, 153 ports, INIT silent.
  const y = await fresh();
  y.sg_init(44100);
  y.sg_set_running(1);
  let peak = 0;
  for (let n = 0; n < 44100; n += 1024) { y.sg_process(1024); for (const v of new Float32Array(y.memory.buffer, y.sg_out_l(), 1024)) peak = Math.max(peak, Math.abs(v)); }
  law.push(["44.1 kHz: rate " + y.sg_sample_rate() + ", latency " + y.sg_latency() + ", params " + y.sg_param_count() + ", ports " + y.sg_port_count() + ", INIT peak " + peak,
    y.sg_sample_rate() === 44100 && y.sg_latency() === 23 && y.sg_param_count() === 454 && y.sg_port_count() === 153 && peak === 0]);
  y.sg_hit(0, 5, 0, 3);
  let hit = 0;
  for (let n = 0; n < 22050; n += 1024) { y.sg_process(1024); for (const v of new Float32Array(y.memory.buffer, y.sg_out_l(), 1024)) hit = Math.max(hit, Math.abs(v)); }
  law.push(["44.1 kHz: BD1 hit peak " + hit.toFixed(4), hit > 0.05]);
}
{
  // Jack ids in every JCS R6 form go through the shared parseJackId inside the wasm engine (sg_port_find).
  const y = await fresh();
  y.sg_init(48000);
  const find = (t) => { const b = new Uint8Array(y.memory.buffer, y.sg_text(), 256); const e = new TextEncoder().encode(t); b.set(e); b[e.length] = 0; return y.sg_port_find(); };
  const id = (i) => { const p = y.sg_port_id(i); const b = new Uint8Array(y.memory.buffer, p, 64); return new TextDecoder().decode(b.subarray(0, b.indexOf(0))); };
  let ok = 0;
  for (let i = 0; i < y.sg_port_count(); ++i) {
    const s = id(i);
    if (find(s) === i && find("SHOGUN/" + s) === i && find("SHOGUN#1/" + s) === i) ++ok;
  }
  const legacy = find("MIX L") >= 0 && find("LFO OUT") >= 0, foreign = find("RONIN#1/BD1:TRIG") === -1;
  law.push(["jack ids: " + ok + "/153 resolve in bare, SHOGUN/ and SHOGUN#1/ forms (e.g. LEAD:V/OCT " + find("SHOGUN#1/LEAD:V/OCT") + "); legacy names " + (legacy ? "ok" : "MISSING") + "; foreign prefix refused " + foreign,
    ok === 153 && legacy && foreign]);
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

// The page's save path in a real browser (headless Chrome): web/shogun.html with the test below appended. For every
// factory program (INIT + the bank): load it, edit one visible control, SAVE, load the saved pattern again, and check
// that every field of the document other than the edited one is identical to the factory document (probability,
// micro-timing, ratchets, p-locks, mod rows and the rest of the kit included), that the saved text is a fixed point,
// that the reloaded panel shows the edit, and that the engine the page plays from holds the saved document.
// Run after `node web/build_page.mjs`; part of `make web`.
import { readFileSync, writeFileSync, mkdtempSync, rmSync, existsSync } from "node:fs";
import { execFileSync } from "node:child_process";
import { tmpdir } from "node:os";

const root = new URL("..", import.meta.url).pathname;
const chrome = ["google-chrome", "google-chrome-stable", "chromium", "chromium-browser"].find((c) => {
  try { execFileSync("which", [c], { stdio: "ignore" }); return true; } catch { return false; }
});
if (!chrome) { console.log("test_page: SKIP (no Chrome or Chromium on PATH)"); process.exit(0); }

// ---- runs inside the page, after its own script (same global scope: FACT, P, MAP, tracks, mirror, ...)
function pageTest() {
  const S = window.SHOGUN, res = { lines: [], fails: [], stats: {} };
  const fail = (m) => res.fails.push(m);
  const docId = (k) => (k == "HH" ? "CH" : k);
  // a document as path -> value, tracks keyed by id and steps by index, so an added step moves nothing else
  function flat(text) {
    const d = JSON.parse(text), out = {};
    const walk = (v, p) => {
      if (v && typeof v == "object" && !Array.isArray(v)) { for (const k in v) walk(v[k], p + "/" + k); }
      else if (Array.isArray(v)) v.forEach((e, i) => walk(e, p + "/" + i));
      else out[p] = v;
    };
    const tr = {};
    for (const t of d.seq.tracks) { const st = {}; for (const s of t.steps) st[s.i] = s; tr[t.id] = { ...t, steps: st }; }
    d.seq = { ...d.seq, tracks: tr };
    walk(d, "");
    return out;
  }
  const diff = (a, b) => [...new Set([...Object.keys(a), ...Object.keys(b)])].filter((k) => a[k] !== b[k]).sort();
  const pageOwned = (k) => /^\/params\/(MASTER:VOLUME|[A-Z0-9]+:SOLO|CLOCK:MODE|CLOCK:SOURCE)$/.test(k) || /^\/(cvAmt|cables)\//.test(k);
  const knobIds = Object.keys(MAP).filter((id) => MAP[id].f && !MAP[id].tog && !MAP[id].master);
  const levelIds = Object.keys(MAP).filter((id) => MAP[id].level != null);
  const stepKey = (k) => CTRL.find((c) => c.step === k && c.kind != "acc");
  const nf = FACT.length;
  let hidden = { rows: 0, prob: 0, micro: 0, ratchet: 0, locks: 0 }, liveOk = 0, fixed = 0, again = 0, shown = 0, clean = 0;
  const kinds = {};
  for (let i = 0; i < nf; i++) {
    S.loadPat("A", i);
    mxLoad({ fx: i });
    const base = mxDoc(), fb = flat(base), bd = JSON.parse(base);
    hidden.rows += bd.mod.length;
    for (const t of bd.seq.tracks) for (const s of t.steps) {
      if (s.prob < 1) hidden.prob++;
      if (s.micro) hidden.micro++;
      if (s.ratchet > 1) hidden.ratchet++;
      if (s.locks && Object.keys(s.locks).length) hidden.locks++;
    }
    if (lfoLines.length != 4) fail(`${FACT[i].n}: the LFO tab lists ${lfoLines.length} LFOs`);
    const lr = lfoRead();
    if (LFOK.some((k) => P[k] !== lr.P[k])) fail(`${FACT[i].n}: the LFO knobs are not the kit's LFO 1`);
    // one visible control
    const kind = ["knob", "step", "tempo", "length", "lfoAmount", "level", "accent", "lfoDiv"][i % 8];
    kinds[kind] = (kinds[kind] || 0) + 1;
    const v = VOICES[(i * 5) % VOICES.length].k, dv = docId(v);
    let allowed, visible, what;
    if (kind == "knob") {
      const id = knobIds[(i * 7) % knobIds.length], to = P[id] < 0.5 ? P[id] + 0.3 : P[id] - 0.3;
      S.setP(id, to);
      what = id;
      allowed = (d) => d.length == 1 && d[0].startsWith("/params/");
      visible = () => Math.abs(P[id] - to) <= 1 / 127 + 1e-9 || (STEPS[id] && Math.abs(P[id] - to) <= 1 / (STEPS[id] - 1));
    } else if (kind == "level") {
      const id = levelIds[(i * 3) % levelIds.length], to = P[id] < 0.5 ? P[id] + 0.25 : P[id] - 0.25;
      S.setP(id, to);
      what = id;
      allowed = (d) => d.length == 1 && /^\/params\/[A-Z0-9]+:LEVEL$/.test(d[0]);
      visible = () => Math.abs(P[id] - to) < 1e-6;
    } else if (kind == "step" || kind == "accent") {
      selTrack(v);
      S.press({ id: "PAGE:0", kind: "key" }, {});
      const k = (i * 3) % 16, was = JSON.stringify(tracks[v].steps[k]);
      S.press(stepKey(k), { shiftKey: kind == "accent", button: 0 });
      const now = JSON.stringify(tracks[v].steps[k]);
      what = `${v} step ${k + 1} ${kind == "accent" ? "accent" : "on/off"}`;
      allowed = (d) => d.length > 0 && d.every((p) => p.startsWith(`/seq/tracks/${dv}/steps/${k}/`));
      visible = () => JSON.stringify(tracks[v].steps[k]) == now && now != was;
    } else if (kind == "tempo") {
      const to = P["CLOCK:TEMPO"] < 0.5 ? P["CLOCK:TEMPO"] + 0.1 : P["CLOCK:TEMPO"] - 0.1;
      S.setP("CLOCK:TEMPO", to);
      what = "TEMPO " + bpmOf(to).toFixed(1);
      allowed = (d) => d.length == 1 && d[0] == "/params/CLOCK:TEMPO";
      visible = () => Math.abs(bpmOf(P["CLOCK:TEMPO"]) - bpmOf(to)) < 0.051;
    } else if (kind == "length") {
      selTrack(v);
      const len = tracks[v].len, nl = len > 8 ? len - 4 : len + 4;
      S.setP("SEQ:LENGTH", (nl - 1) / 31);
      what = `${v} LENGTH ${len} > ${nl}`;
      allowed = (d) => d.length == 1 && d[0] == `/seq/tracks/${dv}/len`;
      visible = () => tracks[v].len == nl;
    } else if (kind == "lfoAmount") {
      const to = P["LFO:AMOUNT"] < 0.5 ? P["LFO:AMOUNT"] + 0.3 : P["LFO:AMOUNT"] - 0.3;
      S.setP("LFO:AMOUNT", to);
      what = "LFO AMOUNT";
      allowed = (d) => d.length == 1 && d[0] == "/params/LFO 1:DEPTH";
      visible = () => Math.abs(P["LFO:AMOUNT"] - to) < 1e-6;
    } else {
      const di = Math.round(P["LFO:DIV"] * 9), to = (di + 3) % 10 / 9;
      S.setP("LFO:DIV", to);
      what = "LFO DIVISION " + LDIV[Math.round(to * 9)][0];
      allowed = (d) => d.length >= 1 && d.every((p) => p == "/params/LFO 1:DIV" || p == "/params/LFO 1:SYNC") && d.includes("/params/LFO 1:DIV");
      visible = () => Math.abs(P["LFO:DIV"] - to) < 1e-9;
    }
    // the engine the page plays from (calls queued before the audio starts), before SAVE
    const h0 = shogunHost(wasmBytes(), 48000, () => {});
    h0.run(fxLog);
    const live0 = h0.json();
    S.savePat("RT " + String(i).padStart(2, "0"));
    const at = S.cur.i, rec = patList("A")[at];
    const d1 = diff(fb, flat(rec.doc));
    if (!allowed(d1)) fail(`${FACT[i].n} (${what}): changed ${JSON.stringify(d1)}`);
    else clean++;
    const dl = diff(flat(live0), flat(rec.doc)).filter((k) => !pageOwned(k));
    if (dl.length) fail(`${FACT[i].n} (${what}): playing engine differs from the save in ${JSON.stringify(dl.slice(0, 6))}`);
    // reload the saved pattern: the panel shows the edit, the text is a fixed point, the engine gets the document
    S.loadPat("A", 0);
    S.loadPat("A", at);
    if (visible()) shown++; else fail(`${FACT[i].n} (${what}): the reloaded panel does not show the edit`);
    mxLoad({ doc: rec.doc });
    if (mxDoc() === rec.doc) fixed++; else fail(`${FACT[i].n}: saved text changes on reload`);
    const h = shogunHost(wasmBytes(), 48000, () => {});
    h.run(fxLog);
    const dr = diff(flat(h.json()), flat(rec.doc)).filter((k) => !pageOwned(k));
    if (!dr.length) liveOk++; else fail(`${FACT[i].n}: reloaded engine differs in ${JSON.stringify(dr.slice(0, 6))}`);
    // SAVE again with no edit: the same document
    S.savePat(rec.n, at - nf);
    if (patList("A")[at].doc === rec.doc) again++; else fail(`${FACT[i].n}: a second save without edits changes the document`);
    res.lines.push(`${String(i).padStart(2)} ${FACT[i].n.padEnd(28)} ${what.padEnd(26)} changed ${d1.length} field${d1.length == 1 ? "" : "s"}: ${d1.join(" ")}`);
  }
  res.stats = { programs: nf, clean, shown, fixed, liveOk, again, hidden, kinds };
  return res;
}

const html = readFileSync(root + "web/shogun.html", "utf8");
const page = html + "<script>\ntry{localStorage.clear()}catch(e){}\n" + pageTest.toString() +
  "\n{let r;try{r=pageTest()}catch(e){r={fails:['exception: '+e.message+' '+(e.stack||'')],lines:[],stats:{}}}" +
  "const pre=document.createElement('pre');pre.id='sgtest';pre.textContent=JSON.stringify(r);document.body.appendChild(pre)}\n</script>\n";
writeFileSync(root + "build/page_test.html", page);
const prof = mkdtempSync(tmpdir() + "/sgpage-");
let dom;
try {
  dom = execFileSync(chrome, ["--headless=new", "--no-sandbox", "--disable-gpu", "--user-data-dir=" + prof, "--allow-file-access-from-files",
    "--virtual-time-budget=60000", "--dump-dom", "file://" + root + "build/page_test.html"], { maxBuffer: 1 << 28, timeout: 300000, stdio: ["ignore", "pipe", "ignore"] }).toString();
} finally { if (existsSync(prof)) rmSync(prof, { recursive: true, force: true }); }
const m = dom.match(/<pre id="sgtest">(.*?)<\/pre>/s);
if (!m) { console.log("test_page: FAIL (no result from the page)"); process.exit(1); }
const r = JSON.parse(m[1].replace(/&lt;/g, "<").replace(/&gt;/g, ">").replace(/&quot;/g, '"').replace(/&amp;/g, "&"));
for (const l of r.lines) console.log("  " + l);
const s = r.stats, h = s.hidden || {};
if (s.programs) console.log(`test_page: ${s.programs} programs: ${s.clean} saves change only the edited field, ${s.shown} reload showing the edit, ` +
  `${s.fixed} saved texts are fixed points, ${s.liveOk} playing engines hold the saved document, ${s.again} re-saves identical; ` +
  `hidden detail carried: ${h.rows} mod rows, ${h.prob} steps with probability, ${h.micro} micro-timed, ${h.ratchet} ratchets, ${h.locks} with p-locks; edits ${JSON.stringify(s.kinds)}`);
for (const f of r.fails) console.log("FAIL " + f);
const ok = !r.fails.length && s.programs >= 22 && s.clean === s.programs && s.shown === s.programs && s.fixed === s.programs && s.liveOk === s.programs && s.again === s.programs;
console.log(ok ? "test_page: the web save round trip keeps every field" : "test_page: FAIL");
process.exit(ok ? 0 : 1);

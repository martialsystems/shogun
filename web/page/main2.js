
// ================= patterns =================
// Drum strings: o = soft, x = medium, X = loud (the three accent levels), . = off, repeated to fill the track.
// Note strings: MIDI note numbers 36 to 72, . = rest, - = tie. len: steps per track, L: per-track lengths, tom: T or C per tom.
// The factory bank: INIT and the same documents as the plugin's program list, read back into panel form (tracks, knobs)
// by the mirror below. Playing one loads the document itself (sg_factory_load), so the engine gets every parameter,
// mod row and step detail, including what this panel does not show.
const FACT=[];
// The mirror: a main-thread instance of the wasm build that is never heard. It holds a pattern's whole document
// (engine/patch.h, the plugin's state format): the panel reads its patterns from it, and SAVE writes through it, so
// what the panel has no control for (probability, micro-timing, ratchets, p-locks, mod rows, the rest of the kit's
// LFOs) passes through unchanged.
let MX=null;const PI={};   // PI: parameter index by id
const latin=t=>unescape(encodeURIComponent(t)),unlatin=b=>{try{return decodeURIComponent(escape(b))}catch(e){return b}};
function mirror(){if(MX)return MX;
  const x=new WebAssembly.Instance(new WebAssembly.Module(wasmBytes()),{env:{sin:Math.sin,cos:Math.cos,tan:Math.tan,exp:Math.exp,exp2:(v)=>Math.pow(2,v),pow:Math.pow,tanh:Math.tanh,log:Math.log,log2:Math.log2,log10:Math.log10,log1p:Math.log1p,atan2:Math.atan2}}).exports;
  const str=p=>{const m=new Uint8Array(x.memory.buffer);let t="";while(m[p])t+=String.fromCharCode(m[p++]);return t};
  x.sg_init(48000);const K={};for(let i=0;i<x.sg_knob_count();i++)K[str(x.sg_knob_name(i))]=i;
  for(let i=0;i<x.sg_param_count();i++)PI[str(x.sg_param_id(i))]=i;
  MX={x,str,K,run(calls){for(const c of calls){if(c[0]=="knob"){if(K[c[1]]!=null)x.sg_set_knob(K[c[1]],c[2])}else if(c[0]=="doc")mxLoad({doc:c[1]});else if(typeof x[c[0]]=="function")x[c[0]](...c.slice(1))}}};return MX}
// the mirror's whole state as a document (every parameter)
function mxDoc(){const x=mirror().x,p=x.sg_state_json(1),n=x.sg_state_json_len(),m=new Uint8Array(x.memory.buffer,p,n);let b="";for(let i=0;i<n;i+=8192)b+=String.fromCharCode.apply(null,m.subarray(i,Math.min(n,i+8192)));return unlatin(b)}
// base: {fx} a factory program, or {doc} a saved document
function mxLoad(base){const x=mirror().x;if(base.fx!=null)return !!x.sg_factory_load(base.fx);
  const b=latin(base.doc),p=x.sg_doc_buf(b.length),m=new Uint8Array(x.memory.buffer,p,b.length);for(let i=0;i<b.length;i++)m[i]=b.charCodeAt(i);return !!x.sg_patch_load(b.length)}
// The page's LFO (main1.js LDIV, LSHAPE) from the kit's LFO 1, and the four LFOs as the kit sets them.
const LSH_OF=[0,1,2,2,3,4];   // engine SIN TRI RAMP SAW SQR S&H -> page SINE TRI SAW SAW SQUARE S+H
function lfoRead(){const x=mirror().x,u=id=>x.sg_param(PI[id]),ch=id=>{const i=PI[id],n=x.sg_param_steps(i);return MX.str(x.sg_param_choices(i)).split("|")[Math.min(n-1,Math.floor(u(id)*n))]};
  const beats=[32,48,64/3,16,24,32/3,8,12,16/3,4,6,8/3,2,3,4/3,1,1.5,2/3,.5,.75,1/3,.25,.375,1/6,.125,.1875,1/12,.0625,.09375,1/24];
  const div=Math.min(29,Math.floor(u("LFO 1:DIV")*30)),cpb=1/beats[div];
  let best=0;LDIV.forEach((d,i)=>{if(Math.abs(Math.log2(d[1]/cpb))<Math.abs(Math.log2(LDIV[best][1]/cpb)))best=i});
  const sh=Math.min(5,Math.floor(u("LFO 1:SHAPE")*6));
  const lines=[1,2,3,4].map(n=>{const L="LFO "+n+":";
    return `LFO ${n}  ${u(L+"SYNC")>.5?"SYNC "+ch(L+"DIV"):"FREE "+(.01*Math.pow(4000,u(L+"RATE"))).toFixed(2)+" HZ"} · ${ch(L+"SHAPE")} · ${ch(L+"POL")} · ${ch(L+"MODE")}`+
      ` · DEPTH ${Math.round(100*u(L+"DEPTH"))}% · PHASE ${Math.round(360*u(L+"PHASE"))}°`});
  return {P:{"LFO:DIV":best/9,"LFO:SHAPE":LSH_OF[sh]/4,"LFO:PHASE":u("LFO 1:PHASE"),"LFO:AMOUNT":u("LFO 1:DEPTH")},lines}}
// The mirror's pattern and sound in panel form.
function readPanel(){const x=mirror().x,K=MX.K,cc=f=>K[f]!=null?x.sg_knob_cc(K[f]):0,t={},knobs={};
  VOICES.forEach(v=>{const n=VI.indexOf(v.k),steps=Array.from({length:32},(_,s)=>{const g=f=>x.sg_get_step(n,s,f),b=g(3);
      return {on:!!g(0),acc:g(1),flam:g(2),bend:b<0?null:b,note:g(4),tie:!!g(5)}});
    t[v.k]={len:x.sg_get_track(n,0),shuffle:x.sg_get_track(n,1),shift:x.sg_get_track(n,2),mute:!!x.sg_get_track(n,3),steps}});
  for(const id in MAP){const m=MAP[id];if(m.master)continue;
    if(m.level!=null)knobs[id]=x.sg_get_level(m.level);else if(m.f)knobs[id]=m.tog?(cc(m.f)>=64?1:0):vOf(id,cc(m.f),STEPS[id])}
  return {bpm:+x.sg_get_tempo().toFixed(1),scale:[0,2,3,1][x.sg_get_scale()],bar:x.sg_get_bar(),
    tom:["ltcMode","mtcMode","htcMode"].map(f=>cc(f)>=64?"C":"T").join(""),tracks:t,knobs,lfo:lfoRead()}}
function factoryBank(){const x=mirror().x,out=[];
  for(let i=0;i<x.sg_factory_count();i++){if(!x.sg_factory_load(i))continue;const name=MX.str(x.sg_factory_name(i));out.push({n:name,kit:name,fx:i,...readPanel()})}
  return out}
let fxLog=null;   // a factory pattern loaded before the audio starts: the calls since, replayed when it starts
const store={get(k){try{return JSON.parse(localStorage.getItem(k)||"null")}catch(e){return null}},set(k,v){try{localStorage.setItem(k,JSON.stringify(v))}catch(e){}}};
const blankStep=()=>({on:false,acc:1,flam:-1,bend:null,note:60,tie:false});
let sel="BD1",page=0,edit=0,dirty=false,tracks={},soloV=null;   // soloV: the soloed track, or null
// One list of up to 999 patterns: the factory patterns, then the saved ones, kept in this browser. (Internally bank "A"; an
// older bank B is merged in at load.)
let USERP={A:[],B:[]},curPat={b:"A",i:0},bankView="A";
const pad3=n=>String(n).padStart(3,"0"),patList=b=>(b=="A"?FACT:[]).concat(USERP[b]);
function loadUser(){const u=store.get("shogun.patterns"),old=store.get("shogun.user2");
  USERP=u&&u.A&&u.B?u:{A:old||[],B:[]};if(USERP.B.length){BMOVE=FACT.length+USERP.A.length;USERP.A=USERP.A.concat(USERP.B);USERP.B=[];saveUser()}}
let BMOVE=-1;
const saveUser=()=>store.set("shogun.patterns",USERP);
function fromFactory(p){const t={};VOICES.forEach(v=>{const len=(p.L&&p.L[v.k])||p.len,steps=Array.from({length:32},blankStep);
    if(NOTEK[v.k]){const tok=((p.nt||{})[v.k]||"").split(" ").filter(Boolean);if(tok.length)for(let i=0;i<len;i++){const s=tok[i%tok.length],st=steps[i];if(s=="-"){st.on=true;st.tie=true;st.note=steps[i-1]?steps[i-1].note:60}else if(s!="."){st.on=true;st.note=+s}}}
    else{const s=p.s[v.k];if(s)for(let i=0;i<len;i++){const c=s[i%s.length];if(c!="."){steps[i].on=true;steps[i].acc=c=="o"?0:c=="x"?1:2}}}
    t[v.k]={len,shuffle:p.sh||0,shift:0,mute:false,steps}});return t}
// The sound a pattern loads: a saved pattern's own knobs, else its kit (kits.js) with the levels measured for that kit.
function patKnobs(p){if(p.knobs)return p.knobs;const kit=kitOf(p.n),lv=LEVEL[p.n]||LEVEL.INIT,k={};
  for(const id in MAP){const m=MAP[id];if(m.level!=null)k[id]=lv[VI[m.level]];else if(m.f&&kit[m.f]!=null)k[id]=m.tog?(kit[m.f]>=64?1:0):vOf(id,kit[m.f],STEPS[id])}return k}
function loadPat(b,i,at){const L=patList(b);if(!L.length){info.textContent=`Bank ${b} is empty · SAVE stores the current pattern there`;return}
  i=(i+L.length)%L.length;curPat={b,i};bankView=b;const p=L[i];if(at!=null)rot0=at;curKit={n:p.kit||p.n,dirty:false};undoReset();tracks=p.tracks?JSON.parse(JSON.stringify(p.tracks)):fromFactory(p);
  P["CLOCK:TEMPO"]=clamp((p.bpm-60)/120);P["CLOCK:SCALE"]=(p.scale==null?2:p.scale)/3;P["CLOCK:BAR"]=((p.bar||p.len)-1)/31;
  ["LTC","MTC","HTC"].forEach((k,j)=>{P[k+":MODE"]=p.tom&&p.tom[j]=="C"?1:0});
  const kn=patKnobs(p);for(const id in kn)if(MAP[id]&&!MAP[id].master){P[id]=kn[id];DEF[id]=kn[id]}   // double-click returns a knob to the pattern's setting
  for(const f in LINKED)syncLinked(LINKED[f][LINKED[f].length-1]);
  if(p.lfo){LFOK.forEach(k=>{P[k]=p.lfo.P[k]});lfoLines=p.lfo.lines}
  page=0;edit=0;dirty=false;loadKnobs();
  if(p.fx!=null||p.doc){fxLog=[];send(patCalls(p));info.textContent=`Pattern ${pad3(i+1)} ${p.n} · ${p.bpm} BPM`;return}
  fxLog=null;const c=[];Object.keys(MAP).forEach(id=>c.push(...knobCalls(id)));send(c);
  sendClock();VOICES.forEach(v=>{sendTrack(v.k);sendSteps(v.k)});info.textContent=`Pattern ${pad3(i+1)} ${p.n} · ${p.bpm} BPM`}
// A factory or saved pattern: the document itself, its steps rotated for a chain, then what the page owns (INT/EXT,
// master, solo, cables). ["doc", text] loads a saved document (host.js).
const patCalls=p=>[p.fx!=null?["sg_factory_load",p.fx]:["doc",p.doc],["sg_rotate",rot0],["sg_commit"],["sg_set_mode",P["CLOCK:SOURCE"]>.5?1:0],
  ...knobCalls("OUT:MASTER"),["sg_set_solo",soloV?VI.indexOf(soloV):-1],...cableCalls()];
// The document a pattern starts from: its factory program, its saved document, or INIT (an older saved pattern).
const baseOf=p=>p.fx!=null?{fx:p.fx}:p.doc?{doc:p.doc}:{fx:0};
// The page panel as P values, the way loadPat sets them from a panel record.
const clockP=r=>({"CLOCK:TEMPO":clamp((r.bpm-60)/120),"CLOCK:SCALE":(r.scale==null?2:r.scale)/3,"CLOCK:BAR":((r.bar||r.len)-1)/31});
// The calls that take a document loaded in the mirror (read back as ref) to what the panel shows now: only the knobs,
// clock settings, track settings, steps and LFO controls that differ, so every other field keeps its exact value.
function editCalls(ref){const c=[],cp=clockP(ref);
  for(const id in MAP)if(!MAP[id].master&&ref.knobs[id]!=null&&P[id]!==ref.knobs[id])c.push(...knobCalls(id));
  if(P["CLOCK:TEMPO"]!==cp["CLOCK:TEMPO"])c.push(["sg_set_param",PI["CLOCK:TEMPO"],(bpmOf(P["CLOCK:TEMPO"])-40)/160]);
  if(P["CLOCK:SCALE"]!==cp["CLOCK:SCALE"])c.push(["sg_set_scale",SPQ[Math.round(P["CLOCK:SCALE"]*3)]]);
  if(P["CLOCK:BAR"]!==cp["CLOCK:BAR"])c.push(["sg_set_bar",1+Math.round(P["CLOCK:BAR"]*31)]);
  VOICES.forEach(v=>{const a=tracks[v.k],b=ref.tracks[v.k],n=VI.indexOf(v.k);
    if(a.len!==b.len||a.shuffle!==b.shuffle||a.shift!==b.shift||a.mute!==b.mute)c.push(...trackCalls(v.k));
    a.steps.forEach((s,i)=>{const r=b.steps[i];if(s.on===r.on&&s.acc===r.acc&&s.flam===r.flam&&s.bend===r.bend&&s.note===r.note&&s.tie===r.tie)return;
      c.push(NOTEK[v.k]?["sg_set_note",n,i,s.on?s.note:-1,s.acc,s.on&&s.tie?1:0]:["sg_set_drum",n,i,s.on?1:0,s.acc,s.flam,s.bend==null?-1:s.bend])})});
  LFOK.forEach(k=>{if(P[k]!==ref.lfo.P[k])c.push(lfoCall(k))});
  return c.concat([["sg_commit"]])}
// The loaded pattern's document with the panel's edits: the base document in the mirror, the factory kit loaded since
// (if any), then editCalls. Returns {doc, panel}: the document text and the panel read back from it.
function patchDoc(p){mxLoad(baseOf(p));if(curKit.fx!=null)mirror().x.sg_factory_kit(curKit.fx);
  MX.run(editCalls(readPanel()));return {doc:mxDoc(),panel:readPanel()}}
// over: the index of the user pattern to write over; otherwise the pattern goes at the end of the list
// The record keeps the whole document (doc, what loads) and the panel read back from it (what the list and the knobs show).
function savePat(name,over){const cp=patList(curPat.b)[curPat.i],d=patchDoc(cp||{fx:0}),knobs=d.panel.knobs;
  const rec={n:name,kit:curKit.n,...d.panel,doc:d.doc};
  if(over!=null)USERP[curPat.b][over]=rec;else{USERP[bankView].push(rec);curPat={b:bankView,i:patList(bankView).length-1}}saveUser();dirty=false;curKit={n:curKit.n,dirty:false};for(const id in knobs)DEF[id]=knobs[id];info.textContent=`Saved ${over!=null?"over":"as"} ${pad3(curPat.i+1)} ${name} in this browser`}
// the TRACK and STEP knobs show the selected track and the edit step
function loadKnobs(){const t=tracks[sel],st=t.steps[edit];P["SEQ:LENGTH"]=(t.len-1)/31;P["SEQ:SHUFFLE"]=t.shuffle/15;P["SEQ:SHIFT"]=t.shift/127;
  P["STEP:FLAM"]=(st.flam+1)/16;P["STEP:BEND"]=st.bend==null?.5:st.bend/127;P["STEP:NOTE"]=(clamp(st.note,36,72)-36)/36}

// ================= the engine: build/shogun.wasm (engine/shogun.cpp) in an AudioWorklet, ScriptProcessor fallback =================
// The page sends batches of calls ([name, ...args]) to the wasm entry points in web/wasm/shogun_web.cpp; ["knob", name, cc] sets a knob by its shogun::Knobs name.
let ctx=null,node=null,host=null,mon=null,running=false,counter=-1;let outbox=[];
const SPQ=[8,6,4,3];
function send(calls){if(node)node.port.postMessage(calls);else if(host)host.run(calls);else{outbox.push(...calls);if(fxLog)fxLog.push(...calls)}}
function onEngine(m){if(m.lfo!=null){lfoV=m.lfo;lfoP=m.lfoP;lfoT=performance.now()}counter=m.running?m.counter:-1;chainTick();if(m.running!=running){running=m.running;$("go").textContent=running?"Stop":"Start"}}
const wasmBytes=()=>Uint8Array.from(atob($("wasm").textContent.trim()),c=>c.charCodeAt(0)).buffer;
function audio(){if(ctx){if(ctx.state=="suspended")ctx.resume();return}
  // The engine runs at 48 kHz (dsp.h kFs). Ask for that rate; the host resamples if the browser will not.
  const AC=window.AudioContext||window.webkitAudioContext;try{ctx=new AC({sampleRate:48000})}catch(e){ctx=new AC()}
  mon=ctx.createGain();mon.gain.value=+$("vol").value;mon.connect(ctx.destination);
  const src=$("host").textContent+"\nclass ShogunProc extends AudioWorkletProcessor{constructor(o){super();this.h=shogunHost(o.processorOptions.bytes,sampleRate,m=>this.port.postMessage(m));this.port.onmessage=e=>this.h.run(e.data)}process(i,o){const c=o[0];this.h.render(c[0],c[1]||c[0],c[0].length);return true}}\nregisterProcessor('shogun',ShogunProc);";
  const worklet=()=>ctx.audioWorklet.addModule(URL.createObjectURL(new Blob([src],{type:"application/javascript"}))).then(()=>{
    node=new AudioWorkletNode(ctx,"shogun",{numberOfInputs:0,outputChannelCount:[2],processorOptions:{bytes:wasmBytes()}});node.port.onmessage=e=>onEngine(e.data);node.connect(mon)});
  const fallback=()=>{node=null;host=shogunHost(wasmBytes(),ctx.sampleRate,onEngine);const sp=ctx.createScriptProcessor(1024,0,2);
    sp.onaudioprocess=e=>{const b=e.outputBuffer;host.render(b.getChannelData(0),b.getChannelData(1),b.length)};sp.connect(mon)};
  (ctx.audioWorklet?worklet():Promise.reject(new Error("no AudioWorklet"))).catch(e=>{console.warn("SHOGUN: AudioWorklet unavailable, using ScriptProcessor:",e&&e.message);fallback()}).then(()=>{outbox=[];syncAll()})}
function knobCalls(id){const m=MAP[id];if(!m)return[];
  if(m.level!=null)return[["sg_set_level",m.level,P[id]]];if(m.master)return[["sg_set_master",P[id]]];
  return[["knob",m.f,m.tog?(P[id]>.5?127:0):ccOf(id,P[id])]]}
const sendParam=id=>send(knobCalls(id));
const clockCalls=()=>[["sg_set_tempo",bpmOf(P["CLOCK:TEMPO"])],["sg_set_scale",SPQ[Math.round(P["CLOCK:SCALE"]*3)]],["sg_set_mode",P["CLOCK:SOURCE"]>.5?1:0],["sg_set_bar",1+Math.round(P["CLOCK:BAR"]*31)],["sg_commit"]];
const sendClock=()=>send(clockCalls());
function trackCalls(k){const t=tracks[k],v=VI.indexOf(k);return[["sg_set_track",v,t.len,t.shuffle,t.shift,t.mute?1:0]]}
// A track plays slot counter % length. A chained pattern starts on counter rot0, so its steps go out rotated by rot0.
let rot0=0;const posOf=(c,len)=>((c-rot0)%len+len)%len;
function stepCalls(k){const t=tracks[k],v=VI.indexOf(k),o=posOf(0,t.len)?t.len-posOf(0,t.len):0,out=t.steps.slice();
  for(let j=0;j<t.len;j++)out[(j+o)%t.len]=t.steps[j];
  return out.map((s,i)=>NOTEK[k]?["sg_set_note",v,i,s.on?s.note:-1,s.acc,s.on&&s.tie?1:0]:["sg_set_drum",v,i,s.on?1:0,s.acc,s.flam,s.bend==null?-1:s.bend])}
const sendTrack=k=>send(trackCalls(k).concat([["sg_commit"]]));
const sendSteps=k=>send(stepCalls(k).concat([["sg_commit"]]));
// Inside this page the gate sources are CLK OUT and ACC OUT; either can drive every Trig jack and RST IN, RUN IN and CLK IN.
// An input can stack: the engine sees it high while any of its sources is high. A Trig cable never moves the INT/EXT switch.
// LFO OUT is the one CV source; the CV inputs (19 to 24) sum what is patched into them (a gate counts as 0 or 5 V).
const INPUT={"CLOCK:RST IN":16,"CLOCK:RUN IN":17,"CLOCK:CLK IN":18,"CV:BD1 PITCH":19,"CV:BD2 PITCH":20,"CV:SD PITCH":21,"CV:TOM PITCH":22,"CV:HAT DECAY":23,"CV:SD SNAPPY":24},
  GATESRC={"SHOGUN/CLOCK:CLK OUT":1,"SHOGUN/CLOCK:ACC OUT":2,"SHOGUN/LFO:LFO OUT":4};
function cableCalls(){const src=new Array(25).fill(0);
  cables.forEach(c=>{if(!c.a||!c.b)return;const[o,i]=JACKS[c.a].dir=="out"?[c.a,c.b]:[c.b,c.a];if(!GATESRC[o])return;
    const id=i.slice(7),n=id.endsWith(":TRIG")?VI.indexOf(id.slice(0,-5)):INPUT[id];if(n!=null&&n>=0)src[n]|=GATESRC[o]});
  return src.map((s,n)=>["sg_patch",n,s])}
const sendCables=()=>send(cableCalls());
// The page LFO is the kit's LFO 1: a pattern loads it with the kit, and a control sends only its own field
// (sg_set_lfo_field), so the rest of LFO 1 (polarity, mode, slew, ...) stays as the kit has it.
const LFOK=["LFO:DIV","LFO:SHAPE","LFO:PHASE","LFO:AMOUNT"];
let lfoLines=[];   // the kit's four LFOs, as text for the LFO tab
const lfoCall=k=>["sg_set_lfo_field",LFOK.indexOf(k),k=="LFO:DIV"?lfoDiv()[1]:k=="LFO:SHAPE"?lfoShapeI():P[k]];
const sendLfo=k=>send([lfoCall(k)]);
function syncAll(){if(fxLog){send(fxLog.concat([["sg_set_running",running?1:0]]));fxLog=null;return}
  const c=[];Object.keys(MAP).forEach(id=>c.push(...knobCalls(id)));c.push(...clockCalls());
  VOICES.forEach(v=>c.push(...trackCalls(v.k),...stepCalls(v.k)));c.push(["sg_commit"],...cableCalls(),["sg_set_solo",soloV?VI.indexOf(soloV):-1],["sg_set_running",running?1:0]);send(c)}
function setRun(on){audio();
  // a start counts from 0: the chain starts from its first pattern, else the steps go out unrotated
  if(on&&!running&&!chainStart()&&rot0){rot0=0;const cp=patList(curPat.b)[curPat.i];
    if(cp&&(cp.fx!=null||cp.doc)&&!dirty){send(patCalls(cp))}else{const c=[];VOICES.forEach(v=>c.push(...stepCalls(v.k)));c.push(["sg_commit"]);send(c)}}
  running=on;if(!on)counter=-1;send([["sg_set_running",on?1:0]]);$("go").textContent=on?"Stop":"Start"}
// A voice key while stopped plays the voice, so a sound can be set without running the pattern.
function audition(k){audio();const v=VI.indexOf(k);if(NOTEK[k]){const st=tracks[k].steps[edit];send([["sg_trigger_note",v,st.note,1]]);setTimeout(()=>send([["sg_release",v]]),300)}else send([["sg_trigger",v,1,0]])}
$("vol").oninput=()=>{if(mon)mon.gain.setTargetAtTime(+$("vol").value,ctx.currentTime,.02)};

// ================= drawing the live parts =================
function put(id,html){if(drawn[id]===html)return;drawn[id]=html;lv(id).innerHTML=html}
const lcdPut=(id,text)=>{const L=LCD[id];put("LCD:"+id,dots(L.x,L.y,L.w,L.h,text,L.n))};
function drawAll(){
  drawViews();
  for(const id in P){const L=LIVE[id];if(!L)continue;if(L.r)put(id,knobBody(L.x,L.y,L.r,-135+270*P[id]));else{const r=P[id]>.5;put(id,`<line x1="${L.x}" y1="${L.y}" x2="${L.x+(r?12:-12)}" y2="${L.y-2}" stroke="#d8d8d2" stroke-width="3.2" stroke-linecap="round"/><circle cx="${L.x+(r?12:-12)}" cy="${L.y-2}" r="3.6" fill="url(#js)"/>`)}}
  const t=tracks[sel],bar=1+Math.round(P["CLOCK:BAR"]*31);
  lcdPut("BPM",bpmOf(P["CLOCK:TEMPO"]).toFixed(0).padStart(3," "));lcdPut("POS",counter>=0&&counter>=rot0?String(posOf(counter,bar)+1).padStart(2," "):"--");lcdPut("LEN",String(t.len).padStart(2," "));lcdPut("EDIT",String(edit+1).padStart(2," "));
  put("KIT",dots(KITR.x+8,KITR.y+6,KITR.w-16,KITR.h-12,kitText().slice(-12),12));put("CHN",dots(CHN.x+8,CHN.y+6,CHN.w-16,CHN.h-12,chainText(),8));
  put("SCR",dots(SCR.x+8,SCR.y+6,SCR.w-16,SCR.h-12,scrText().slice(-20),20));put("TRK",dots(TRK.x+8,TRK.y+6,TRK.w-16,TRK.h-12,trkText(),8));
  const kb=(id,black)=>{const L=LIVE[id];put(id,keyBody(L.x,L.y,black,pressed==id))},ld=(id,on,c)=>{const L=LIVE[id];put(id,lamp(L.x,L.y,L.r,on,c))};
  VOICES.forEach(v=>ld("LED:"+v.k,v.k==sel));ld("LED:LEARN",MIDI.learn);
  kb("START",false);kb("CLEAR",true);kb("UNDO",true);kb("RANDOM",true);kb("SOLO",true);ld("LED:SOLO",soloV!=null&&soloV==sel);kb("BAY",true);ld("LED:BAY",bay);ld("LED:RUN",running);kb("MUTE",true);ld("LED:MUTE",t.mute);kb("TIE",true);ld("LED:TIE",t.steps[edit].tie);
  [0,1].forEach(n=>{kb("PAGE:"+n,true);ld("LED:P"+n,page==n)});
  const o=page*16,ph=running&&counter>=0&&counter>=rot0?posOf(counter,t.len):-1;
  for(let i=0;i<16;i++){const n=o+i,st=t.steps[n],in_=n<t.len,L=LIVE["STEP:"+i];
    put("NUM:"+i,`<text x="${LIVE["NUM:"+i].x}" y="${LIVE["NUM:"+i].y}" font-family="'Liberation Sans',Arial,Helvetica,sans-serif" font-size="12" font-weight="700" letter-spacing=".5" text-anchor="middle" fill="${INK}" opacity="${in_?1:.4}">${n+1}</text>`);
    put("STEP:"+i,`<g opacity="${in_?1:.4}">`+keyBody(L.x,L.y,!st.on,pressed=="STEP:"+i)+`</g>`);
    const a=LIVE["ACC:"+i];put("ACC:"+i,lamp(a.x,a.y,a.r,st.on,["s","g","r"][st.acc]));
    const k=LIVE["LK:"+i];put("LK:"+i,`<text x="${k.x}" y="${k.y}" font-size="9" font-weight="700" text-anchor="middle" fill="${INK}" opacity="${in_?1:.4}">${st.on?lockText(st):""}</text>`);const p=LIVE["PH:"+i];put("PH:"+i,lamp(p.x,p.y,5,ph==n))}}

// the LFO tab: its screens and the scope (drawn only while the tab is up)
function drawViews(){
  if(view=="lfo"){const d=lfoDiv(),i=lfoShapeI();lcdPut("LDIV",d[0].padStart(5," "));lcdPut("LSHAPE",LSHAPE[i]);lcdPut("LPHASE",P["LFO:PHASE"].toFixed(2));
    lcdPut("LAMT",String(Math.round(P["LFO:AMOUNT"]*100)).padStart(3," ")+" ");lcdPut("LRATE",(lfoHz().toFixed(2)+" HZ").padStart(8," "));
    // two cycles of the shape, each from phase 0 to 1; a reset (saw, square, a new held level) is a vertical line
    const g=LIVE["LFO:SCOPE"],a=P["LFO:AMOUNT"],yv=v=>(g.y+g.h-v/5*g.h).toFixed(1),xy=(c,p)=>(g.x+g.w*(c+p)/2).toFixed(1)+","+yv(a*2.5*(1+lfoShapeAt(i,p,c)));
    const E=1e-6,pts=[];for(let c=0;c<2;c++){for(let k=0;k<240;k++){const p=k/240;if(k==120)pts.push(xy(c,.5-E));pts.push(xy(c,p))}pts.push(xy(c,1-E))}
    // the playhead: where the LFO is in its cycle now, on the trace; it alternates between the two drawn cycles
    let mk="";
    if(lfoT>=0){const q=lfoP+lfoHz()*(performance.now()-lfoT)/1000,p=q-Math.floor(q);if(p<lfoLastP-.5)lfoCyc^=1;lfoLastP=p;
      mk=`<circle cx="${xy(lfoCyc,p).split(",")[0]}" cy="${xy(lfoCyc,p).split(",")[1]}" r="5" fill="#ff4a36" stroke="#2a0805" stroke-width="1"/>`}
    put("LFO:SCOPE",`<polyline points="${pts.join(" ")}" fill="none" stroke="#3fe06a" stroke-width="2" stroke-linejoin="miter" opacity=".9"/>`+mk+
      `<text x="${g.x+g.w}" y="${g.y-1}" font-family="'Liberation Sans',Arial,Helvetica,sans-serif" font-size="9" font-weight="700" text-anchor="end" fill="#6f7a66">TWO CYCLES · ${d[0]} · ${LSHAPE[i]}</text>`);
    const kl=LIVE["LFO:KIT"];put("LFO:KIT",lfoLines.map((t,n)=>`<text x="${kl.x}" y="${kl.y+16*n}" font-family="'Liberation Mono','DejaVu Sans Mono',monospace" font-size="11" font-weight="700" fill="${n?"#9a9a90":"#3fe06a"}">${t.replace(/&/g,"&amp;").replace(/</g,"&lt;")}</text>`).join(""))}}

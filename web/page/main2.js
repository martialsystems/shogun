
// ================= patterns =================
// Drum strings: o = soft, x = medium, X = loud (the three accent levels), . = off, repeated to fill the track.
// Note strings: MIDI note numbers 36 to 72, . = rest, - = tie. len: steps per track, L: per-track lengths, tom: T or C per tom.
const FACT=[   // INIT is one empty bar; the factory bank (engine/factory.h) follows it, read from the wasm build at load
 {n:"INIT",bpm:120,sh:0,len:16,tom:"TTT",s:{}}];
// The factory bank: the same documents as the plugin's program list, loaded into a main-thread instance of the wasm
// build and read back into panel form (tracks, knobs). Playing one loads the document itself (sg_factory_load), so the
// engine gets every parameter, mod row and step detail, including what this panel does not show.
function factoryBank(){
  const x=new WebAssembly.Instance(new WebAssembly.Module(wasmBytes()),{env:{sin:Math.sin,cos:Math.cos,tan:Math.tan,exp:Math.exp,exp2:(v)=>Math.pow(2,v),pow:Math.pow,tanh:Math.tanh,log:Math.log,log2:Math.log2,log10:Math.log10,log1p:Math.log1p,atan2:Math.atan2}}).exports;
  const str=p=>{const m=new Uint8Array(x.memory.buffer);let t="";while(m[p])t+=String.fromCharCode(m[p++]);return t};
  x.sg_init(48000);const K={};for(let i=0;i<x.sg_knob_count();i++)K[str(x.sg_knob_name(i))]=i;
  const cc=f=>K[f]!=null?x.sg_knob_cc(K[f]):0,out=[];
  for(let i=1;i<x.sg_factory_count();i++){if(!x.sg_factory_load(i))continue;
    const t={},knobs={};
    VOICES.forEach(v=>{const n=VI.indexOf(v.k),steps=Array.from({length:32},(_,s)=>{const g=f=>x.sg_get_step(n,s,f),b=g(3);
        return {on:!!g(0),acc:g(1),flam:g(2),bend:b<0?null:b,note:g(4),tie:!!g(5)}});
      t[v.k]={len:x.sg_get_track(n,0),shuffle:x.sg_get_track(n,1),shift:x.sg_get_track(n,2),mute:!!x.sg_get_track(n,3),steps}});
    for(const id in MAP){const m=MAP[id];if(m.master)continue;
      if(m.level!=null)knobs[id]=x.sg_get_level(m.level);else if(m.f)knobs[id]=m.tog?(cc(m.f)>=64?1:0):vOf(id,cc(m.f),STEPS[id])}
    const name=str(x.sg_factory_name(i));
    out.push({n:name,kit:name,fx:i,bpm:+x.sg_get_tempo().toFixed(1),scale:[0,2,3,1][x.sg_get_scale()],bar:x.sg_get_bar(),
      tom:["ltcMode","mtcMode","htcMode"].map(f=>cc(f)>=64?"C":"T").join(""),tracks:t,knobs})}
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
  page=0;edit=0;dirty=false;loadKnobs();
  if(p.fx!=null){fxLog=[];send(factoryCalls(p.fx));info.textContent=`Pattern ${pad3(i+1)} ${p.n} · ${p.bpm} BPM`;return}
  fxLog=null;const c=[];Object.keys(MAP).forEach(id=>c.push(...knobCalls(id)));send(c);
  sendClock();VOICES.forEach(v=>{sendTrack(v.k);sendSteps(v.k)});info.textContent=`Pattern ${pad3(i+1)} ${p.n} · ${p.bpm} BPM`}
// A factory pattern: the document itself, its steps rotated for a chain, then what the page owns (clock source, master,
// solo, cables).
const factoryCalls=fx=>[["sg_factory_load",fx],["sg_rotate",rot0],["sg_commit"],["sg_set_mode",P["CLOCK:SOURCE"]>.5?1:0],
  ...knobCalls("OUT:MASTER"),["sg_set_solo",soloV?VI.indexOf(soloV):-1],...cableCalls()];
// over: the index of the user pattern to write over; otherwise the pattern goes at the end of the list
function savePat(name,over){const knobs={};for(const id in MAP)if(!MAP[id].master)knobs[id]=P[id];
  const rec={n:name,kit:curKit.n,bpm:+bpmOf(P["CLOCK:TEMPO"]).toFixed(1),scale:Math.round(P["CLOCK:SCALE"]*3),bar:1+Math.round(P["CLOCK:BAR"]*31),
    tom:["LTC","MTC","HTC"].map(k=>P[k+":MODE"]>.5?"C":"T").join(""),tracks:JSON.parse(JSON.stringify(tracks)),knobs};
  if(over!=null)USERP[curPat.b][over]=rec;else{USERP[bankView].push(rec);curPat={b:bankView,i:patList(bankView).length-1}}saveUser();dirty=false;for(const id in knobs)DEF[id]=knobs[id];info.textContent=`Saved ${over!=null?"over":"as"} ${pad3(curPat.i+1)} ${name} in this browser`}
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
const lfoCalls=()=>[["sg_set_lfo",lfoDiv()[1],P["LFO:PHASE"],lfoShapeI(),P["LFO:AMOUNT"]]];
const LFOK=["LFO:DIV","LFO:SHAPE","LFO:PHASE","LFO:AMOUNT"];
function sendLfo(){send(lfoCalls());store.set("shogun.lfo",Object.fromEntries(LFOK.map(k=>[k,P[k]])))}
function loadLfo(){const o=store.get("shogun.lfo")||{};LFOK.forEach(k=>{if(typeof o[k]=="number")P[k]=clamp(o[k])})}
function syncAll(){if(fxLog){send(fxLog.concat([["sg_set_running",running?1:0]]));fxLog=null;return}
  const c=[];Object.keys(MAP).forEach(id=>c.push(...knobCalls(id)));c.push(...clockCalls());
  VOICES.forEach(v=>c.push(...trackCalls(v.k),...stepCalls(v.k)));c.push(["sg_commit"],...cableCalls(),...lfoCalls(),["sg_set_solo",soloV?VI.indexOf(soloV):-1],["sg_set_running",running?1:0]);send(c)}
function setRun(on){audio();
  // a start counts from 0: the chain starts from its first pattern, else the steps go out unrotated
  if(on&&!running&&!chainStart()&&rot0){rot0=0;const cp=patList(curPat.b)[curPat.i];
    if(cp&&cp.fx!=null&&!dirty){send(factoryCalls(cp.fx))}else{const c=[];VOICES.forEach(v=>c.push(...stepCalls(v.k)));c.push(["sg_commit"]);send(c)}}
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
      `<text x="${g.x+g.w}" y="${g.y-1}" font-family="'Liberation Sans',Arial,Helvetica,sans-serif" font-size="9" font-weight="700" text-anchor="end" fill="#6f7a66">TWO CYCLES · ${d[0]} · ${LSHAPE[i]}</text>`)}}

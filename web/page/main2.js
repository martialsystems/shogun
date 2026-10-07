
// ================= patterns =================
// Drum strings: o = soft, x = medium, X = loud (the three accent levels), . = off, repeated to fill the track.
// Note strings: MIDI note numbers 36 to 72, . = rest, - = tie. len: steps per track, L: per-track lengths, tom: T or C per tom.
const FACT=[
 {n:"HOUSE",bpm:124,sh:2,len:32,tom:"TTT",s:{BD1:"X...X...X...X...",CP:"....X.......X.......X.......X.XX",OH:"..x...x...x...x.",HH:"x.o.x.o.x.o.x.o.",RS:"...........x...................x",MA:".o.o.o.o.o.o.o.o"},nt:{BASS:". . 36 . . . 36 . . . 36 . . . 39 ."}},
 {n:"TECHNO",bpm:132,sh:0,len:16,L:{RS:12,CP:24},tom:"TTT",s:{BD1:"X...X...X...X...",RS:"...x..x....x",HH:"oxXoxoXooxXoxoXo",OH:"..x...x...x...x.",CY:"X...............",CP:"....x.......x...........x"},nt:{BASS:"36 36 . 36 36 . 36 36 . 36 36 . 36 . 36 ."}},
 {n:"ELECTRO",bpm:118,sh:0,len:16,tom:"TTT",s:{BD1:"X......X..X.....",SD:"....X.......X...",HH:"x.o.x.o.x.o.xxx.",CL:"..x..x....x..x..",CB:"...........x..x.",CP:"....X.......X..."},nt:{BASS:"36 . . 36 . . 43 . 36 . . 36 . 46 . 48"}},
 {n:"BREAKBEAT",bpm:136,sh:1,len:16,tom:"TTT",s:{BD1:"X.x.......Xx....",SD:"....X..o.o..X..o",HH:"x.x.x.x.x.x.x.x.",OH:"..............x.",CY:"X..............."}},
 {n:"HIP HOP",bpm:90,sh:7,len:16,tom:"TTT",s:{BD2:"X......x.xX.....",SD:"....X.......X...",HH:"x.o.x.o.x.oxo.x.",OH:".............x..",RS:"..........x....."}},
 {n:"MIAMI BASS",bpm:128,sh:0,len:16,tom:"TTT",s:{BD2:"X.....x...X.....",CP:"....X.......X...",HH:"oxXoxoXooxXoxoXo",CL:"...x..x......x..",CB:"..............x."}},
 {n:"ROBOT POP",bpm:112,sh:0,len:16,tom:"TTT",s:{BD1:"X...X...X...X...",SD:"....X.......X...",HH:"xoxoxoxoxoxoxoxo",LTC:"...........x....",MTC:".............x..",HTC:"..............x.",CL:"x..x..x.........",MA:"o.o.o.o.o.o.o.o."},nt:{LEAD:"60 - 63 . 67 - 70 . 72 . 70 67 63 - 62 .",BASS:"36 . 36 48 36 . 36 48 39 . 39 51 34 . 34 46"}},
 {n:"LATIN",bpm:104,sh:3,len:16,tom:"CCC",s:{BD1:"x..x....x..x....",CL:"x..x...x..x.x...",CB:"x.o.xo.x.o.xo.x.",MA:"oxoxoxoxoxoxoxox",HTC:"..x...xX..x...xX",MTC:"x...x.....x.....",LTC:".......x.....x.."}},
 {n:"DISCO",bpm:120,sh:0,len:16,tom:"TTT",s:{BD1:"X...X...X...X...",SD:"....X.......X...",OH:"..x...x...x...x.",HH:"xo.xox.xox.xox.o",CP:"....x.......x...",CB:"x.......x......."},nt:{BASS:"36 48 36 48 36 48 36 48 41 53 41 53 43 55 43 55"}},
 {n:"MINIMAL",bpm:126,sh:4,len:16,L:{MA:6,CB:10,RS:14,BD2:32},tom:"TTT",s:{BD1:"X...X...X...X...",RS:"......x..x..x.",OH:"..x...x...x...x.",MA:"x.o.xo",CB:"...x....x.",BD2:"...............................x"}},
 {n:"THREE FOUR",bpm:96,sh:0,len:12,tom:"TTT",s:{BD2:"X.....x.....",SD:"....x.....x.",HH:"x.o.x.o.x.o.",CY:"X..........."}},
 {n:"EMPTY",bpm:120,sh:0,len:16,tom:"TTT",s:{}}];
const store={get(k){try{return JSON.parse(localStorage.getItem(k)||"null")}catch(e){return null}},set(k,v){try{localStorage.setItem(k,JSON.stringify(v))}catch(e){}}};
const blankStep=()=>({on:false,acc:1,flam:-1,bend:null,note:60,tie:false});
let PATS=[],pi=0,sel="BD1",page=0,edit=0,dirty=false,tracks={};
function loadPats(){const u=store.get("shogun.user2")||[];PATS=FACT.map((p,i)=>({...p,id:"A"+String(i+1).padStart(2,"0")})).concat(u.map((p,i)=>({...p,id:"U"+String(i+1).padStart(2,"0")})))}
function fromFactory(p){const t={};VOICES.forEach(v=>{const len=(p.L&&p.L[v.k])||p.len,steps=Array.from({length:32},blankStep);
    if(NOTEK[v.k]){const tok=((p.nt||{})[v.k]||"").split(" ").filter(Boolean);if(tok.length)for(let i=0;i<len;i++){const s=tok[i%tok.length],st=steps[i];if(s=="-"){st.on=true;st.tie=true;st.note=steps[i-1]?steps[i-1].note:60}else if(s!="."){st.on=true;st.note=+s}}}
    else{const s=p.s[v.k];if(s)for(let i=0;i<len;i++){const c=s[i%s.length];if(c!="."){steps[i].on=true;steps[i].acc=c=="o"?0:c=="x"?1:2}}}
    t[v.k]={len,shuffle:p.sh||0,shift:0,mute:false,steps}});return t}
function applyPat(i){pi=(i+PATS.length)%PATS.length;const p=PATS[pi];tracks=p.tracks?JSON.parse(JSON.stringify(p.tracks)):fromFactory(p);
  P["CLOCK:TEMPO"]=clamp((p.bpm-60)/120);P["CLOCK:SCALE"]=(p.scale==null?2:p.scale)/3;P["CLOCK:BAR"]=((p.bar||p.len)-1)/31;
  ["LTC","MTC","HTC"].forEach((k,j)=>{P[k+":MODE"]=p.tom&&p.tom[j]=="C"?1:0;sendParam(k+":MODE")});page=0;edit=0;dirty=false;loadKnobs();
  sendClock();VOICES.forEach(v=>{sendTrack(v.k);sendSteps(v.k)});info.textContent=`Pattern ${p.id} ${p.n} · ${p.bpm} BPM`}
function savePat(){const u=store.get("shogun.user2")||[];u.push({n:"USER "+(u.length+1),bpm:+bpmOf(P["CLOCK:TEMPO"]).toFixed(1),scale:Math.round(P["CLOCK:SCALE"]*3),bar:1+Math.round(P["CLOCK:BAR"]*31),
  tom:["LTC","MTC","HTC"].map(k=>P[k+":MODE"]>.5?"C":"T").join(""),tracks});store.set("shogun.user2",u);loadPats();pi=PATS.length-1;dirty=false;info.textContent=`Saved as ${PATS[pi].id} ${PATS[pi].n} in this browser`}
// the TRACK and STEP knobs show the selected track and the edit step
function loadKnobs(){const t=tracks[sel],st=t.steps[edit];P["SEQ:LENGTH"]=(t.len-1)/31;P["SEQ:SHUFFLE"]=t.shuffle/15;P["SEQ:SHIFT"]=t.shift/127;
  P["STEP:FLAM"]=(st.flam+1)/16;P["STEP:BEND"]=st.bend==null?.5:st.bend/127;P["STEP:NOTE"]=(clamp(st.note,36,72)-36)/36}

// ================= the engine: build/shogun.wasm (engine/shogun.cpp) in an AudioWorklet, ScriptProcessor fallback =================
// The page sends batches of calls ([name, ...args]) to the wasm entry points in web/wasm/shogun_web.cpp; ["knob", name, cc] sets a knob by its shogun::Knobs name.
let ctx=null,node=null,host=null,mon=null,running=false,counter=-1;let outbox=[];
const SPQ=[8,6,4,3];
function send(calls){if(node)node.port.postMessage(calls);else if(host)host.run(calls);else outbox.push(...calls)}
function onEngine(m){counter=m.running?m.counter:-1;if(m.running!=running){running=m.running;$("go").textContent=running?"Stop":"Start"}}
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
function stepCalls(k){const t=tracks[k],v=VI.indexOf(k);
  return t.steps.map((s,i)=>NOTEK[k]?["sg_set_note",v,i,s.on?s.note:-1,s.acc,s.on&&s.tie?1:0]:["sg_set_drum",v,i,s.on?1:0,s.acc,s.flam,s.bend==null?-1:s.bend])}
const sendTrack=k=>send(trackCalls(k).concat([["sg_commit"]]));
const sendSteps=k=>send(stepCalls(k).concat([["sg_commit"]]));
// Inside this page the only gate source is CLK OUT; it can drive every TRIG jack and RST IN, RUN IN and CLK IN.
const INPUT={"CLOCK:RST IN":16,"CLOCK:RUN IN":17,"CLOCK:CLK IN":18};
function cableCalls(){const src=new Array(19).fill(0);
  cables.forEach(c=>{if(!c.a||!c.b)return;const[o,i]=JACKS[c.a].dir=="out"?[c.a,c.b]:[c.b,c.a];if(o!="SHOGUN/CLOCK:CLK OUT")return;
    const id=i.slice(7),n=id.endsWith(":TRIG")?VI.indexOf(id.slice(0,-5)):INPUT[id];if(n!=null&&n>=0)src[n]=1});
  return src.map((s,n)=>["sg_patch",n,s])}
const sendCables=()=>send(cableCalls());
function syncAll(){const c=[];Object.keys(MAP).forEach(id=>c.push(...knobCalls(id)));c.push(...clockCalls());
  VOICES.forEach(v=>c.push(...trackCalls(v.k),...stepCalls(v.k)));c.push(["sg_commit"],...cableCalls(),["sg_set_running",running?1:0]);send(c)}
function setRun(on){audio();running=on;if(!on)counter=-1;send([["sg_set_running",on?1:0]]);$("go").textContent=on?"Stop":"Start"}
// A voice key while stopped plays the voice, so a sound can be set without running the pattern.
function audition(k){audio();const v=VI.indexOf(k);if(NOTEK[k]){const st=tracks[k].steps[edit];send([["sg_trigger_note",v,st.note,1]]);setTimeout(()=>send([["sg_release",v]]),300)}else send([["sg_trigger",v,1,0]])}
$("vol").oninput=()=>{if(mon)mon.gain.setTargetAtTime(+$("vol").value,ctx.currentTime,.02)};

// ================= drawing the live parts =================
function put(id,html){if(drawn[id]===html)return;drawn[id]=html;lv(id).innerHTML=html}
const lcdPut=(id,text)=>{const L=LCD[id];put("LCD:"+id,dots(L.x,L.y,L.w,L.h,text,L.n))};
function drawAll(){
  for(const id in P){const L=LIVE[id];if(!L)continue;if(L.r)put(id,knobBody(L.x,L.y,L.r,-135+270*P[id]));else{const r=P[id]>.5;put(id,`<line x1="${L.x}" y1="${L.y}" x2="${L.x+(r?12:-12)}" y2="${L.y-2}" stroke="#d8d8d2" stroke-width="3.2" stroke-linecap="round"/><circle cx="${L.x+(r?12:-12)}" cy="${L.y-2}" r="3.6" fill="url(#js)"/>`)}}
  const t=tracks[sel],bar=1+Math.round(P["CLOCK:BAR"]*31);
  lcdPut("BPM",bpmOf(P["CLOCK:TEMPO"]).toFixed(0).padStart(3," "));lcdPut("POS",counter>=0?String(counter%bar+1).padStart(2," "):"--");lcdPut("LEN",String(t.len).padStart(2," "));lcdPut("EDIT",String(edit+1).padStart(2," "));
  const pat=PATS[pi];put("SCR",dots(SCR.x+8,SCR.y+6,SCR.w-16,SCR.h-12,(pat.id+" "+pat.n+(dirty?"*":"")).slice(0,17),17));put("TRK",dots(TRK.x+8,TRK.y+6,TRK.w-16,TRK.h-12,VK[sel].t,7));
  const kb=(id,black)=>{const L=LIVE[id];put(id,keyBody(L.x,L.y,black,pressed==id))},ld=(id,on,c)=>{const L=LIVE[id];put(id,lamp(L.x,L.y,L.r,on,c))};
  VOICES.forEach(v=>{kb("SEL:"+v.k,true);ld("LED:"+v.k,v.k==sel)});
  kb("START",false);kb("CLEAR",true);ld("LED:RUN",running);kb("MUTE",true);ld("LED:MUTE",t.mute);kb("TIE",true);ld("LED:TIE",t.steps[edit].tie);
  [0,1].forEach(n=>{kb("PAGE:"+n,true);ld("LED:P"+n,page==n)});
  const o=page*16,ph=running&&counter>=0?counter%t.len:-1;
  for(let i=0;i<16;i++){const n=o+i,st=t.steps[n],in_=n<t.len,L=LIVE["STEP:"+i];
    put("NUM:"+i,`<text x="${LIVE["NUM:"+i].x}" y="${LIVE["NUM:"+i].y}" font-family="'Liberation Sans',Arial,Helvetica,sans-serif" font-size="12" font-weight="700" letter-spacing=".5" text-anchor="middle" fill="${INK}" opacity="${in_?1:.4}">${n+1}</text>`);
    put("STEP:"+i,`<g opacity="${in_?1:.4}">`+keyBody(L.x,L.y,!st.on,pressed=="STEP:"+i)+`</g>`);
    const a=LIVE["ACC:"+i];put("ACC:"+i,lamp(a.x,a.y,a.r,st.on&&st.acc>0,st.acc==1?"g":"r"));const p=LIVE["PH:"+i];put("PH:"+i,lamp(p.x,p.y,5,ph==n))}}

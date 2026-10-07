// ================= step locks, undo, kits, random, chain, MIDI =================
// What sits under a step key: the flam and the bend on a drum track, the note (and ~ for a tie) on LEAD and BASS.
const bendText=b=>{const v=Math.round(12*(2*b/127-1));return (v>=0?"+":"")+v};
function lockText(st){if(NOTEK[sel])return noteName(st.note)+(st.tie?"~":"");
  return [st.flam>=0&&!NOFLAM[sel]?"F"+(st.flam+1):"",st.bend!=null&&BENDK[sel]?bendText(st.bend):""].filter(Boolean).join(" ")}

// Undo: one entry per knob move (a drag, or a run of scroll clicks on one knob) or per key press. Loading a pattern starts over.
const UNDO=[],REDO=[];let undoKey=null,undoT=0;
const snap=()=>({P:{...P},tracks:JSON.parse(JSON.stringify(tracks)),kit:{...curKit},dirty});
function undoPush(key){const now=performance.now();if(key&&key==undoKey&&now-undoT<1000){undoT=now;return}
  undoKey=key;undoT=now;UNDO.push(snap());if(UNDO.length>200)UNDO.shift();REDO.length=0}
function undoReset(){UNDO.length=0;REDO.length=0;undoKey=null}
function restore(s){Object.assign(P,s.P);tracks=s.tracks;curKit=s.kit;dirty=s.dirty;if(edit>=tracks[sel].len)edit=0;loadKnobs();
  const c=[];Object.keys(MAP).forEach(id=>c.push(...knobCalls(id)));c.push(...clockCalls());VOICES.forEach(v=>c.push(...trackCalls(v.k),...stepCalls(v.k)));c.push(["sg_commit"]);send(c);drawAll()}
function undo(redo){const from=redo?REDO:UNDO,to=redo?UNDO:REDO;if(!from.length){info.textContent=redo?"Nothing to redo":"Nothing to undo";return}
  to.push(snap());restore(from.pop());undoKey=null;info.textContent=(redo?"Redo":"Undo")+" · "+UNDO.length+" more to undo, "+REDO.length+" to redo"}

// Kits: the knob settings without the steps. The factory kits are the ones the factory patterns load; saved kits stay in this browser.
let USERK=[],curKit={n:"HOUSE",dirty:false};
const FKITS=Object.keys(STYLE).map(n=>({n:n=="EMPTY"?"BASIC":n,f:n})),kitList=()=>FKITS.concat(USERK);
const kitKnobs=k=>k.knobs||patKnobs({n:k.f});
const panelKnobs=()=>{const k={};for(const id in MAP)if(!MAP[id].master)k[id]=P[id];return k};
function loadKit(i){const k=kitList()[i];if(!k)return;undoPush();const kn=kitKnobs(k);
  for(const id in kn)if(MAP[id]&&!MAP[id].master){P[id]=kn[id];DEF[id]=kn[id]}
  curKit={n:k.n,dirty:false};dirty=true;const c=[];Object.keys(MAP).forEach(id=>c.push(...knobCalls(id)));send(c);drawAll();info.textContent=`Kit ${k.n} · the steps are unchanged`}
function saveKit(name){const rec={n:name,knobs:panelKnobs()},j=USERK.findIndex(k=>k.n==name);if(j>=0)USERK[j]=rec;else USERK.push(rec);
  store.set("shogun.kits",USERK);curKit={n:name,dirty:false};drawAll();info.textContent=`Saved kit ${name} in this browser`}

// RANDOM rolls the selected voice's knobs (not its LEVEL), each inside the range below (controller values; full range if not listed).
const RND={bd1Decay:[25,110],bd1Pitch:[10,110],bd1Tune:[10,80],bd1Noise:[0,70],bd1Filter:[30,127],bd2Tune:[10,90],bd2Decay:[25,115],bd2Tone:[10,127],
  sdTune:[20,110],sdDTune:[30,127],sdPitch:[0,90],sdTone:[10,120],sdToneDecay:[10,90],sdSnappy:[40,127],sdSnDecay:[15,90],rsTune:[20,110],
  cpAttack:[40,127],cpDecay:[10,80],cpFilter:[30,110],clTune:[20,120],clDecay:[10,70],cyTune:[20,110],cyTone:[20,110],cyDecay:[30,110],ohDecay:[15,90],
  hhTune:[30,120],hhDecay:[10,70],ltcTune:[15,100],ltcDecay:[15,90],mtcTune:[15,100],mtcDecay:[15,90],htcTune:[15,100],htcDecay:[15,90],
  cbTune:[20,100],cbDecay:[15,90],maDecay:[5,60],leadTone:[20,120],bassTone:[15,110]};
function randomVoice(){const v=VK[sel],done=[];undoPush();
  v.knobs.forEach(([lb,p,n])=>{if(!p||n=="tog")return;const id=v.k+":"+lb,[lo,hi]=RND[p]||[0,127];P[id]=vOf(id,lo+Math.floor(Math.random()*(hi-lo+1)),STEPS[id]);sendParam(id);done.push(lb)});
  curKit.dirty=true;dirty=true;drawAll();info.textContent=`${v.t} · rolled ${done.join(", ")} · UNDO puts it back`;if(!running)audition(sel)}

// Chain: a list of patterns that play one bar each, in order, while CHAIN is on. Kept in this browser.
let CHAIN={list:[],on:false},chainPos=-1,chainArm=-1;
const saveChain=()=>store.set("shogun.chain",CHAIN);
function loadChain(){const c=store.get("shogun.chain");if(c&&Array.isArray(c.list))CHAIN={list:c.list.map(e=>e.b=="B"?{b:"A",i:BMOVE>=0?BMOVE+e.i:e.i}:e),on:!!c.on};if(BMOVE>=0)saveChain()}
const chainName=e=>{const p=patList(e.b)[e.i];return pad3(e.i+1)+(p?" "+p.n:"")};
function chainText(){const n=CHAIN.list.length;if(!n)return "EMPTY";return CHAIN.on?(running&&chainPos>=0?(chainPos+1)+"/"+n:"ON "+n):"OFF "+n}
// On the last step of the bar, the next pattern is sent to start on the next step.
function chainTick(){if(!CHAIN.on||!running||counter<0||!CHAIN.list.length)return;const bar=1+Math.round(P["CLOCK:BAR"]*31);
  if(posOf(counter,bar)!=bar-1||chainArm==counter)return;chainArm=counter;chainPos=(chainPos+1)%CHAIN.list.length;const e=CHAIN.list[chainPos];
  if(e.b!=curPat.b||e.i!=curPat.i)loadPat(e.b,e.i,counter+1)}
function chainStart(){if(!CHAIN.on||!CHAIN.list.length)return false;chainPos=0;chainArm=-1;const e=CHAIN.list[0];loadPat(e.b,e.i,0);return true}

// MIDI: a note per voice, notes 36 to 51 in voice order to start (the plugin's map). LEARN, then a voice key, then a pad.
const MIDI={ok:false,learn:false,map:{},access:null};
const midiDefault=()=>Object.fromEntries(VI.map((k,i)=>[k,36+i]));
function midiLoad(){MIDI.map=Object.assign(midiDefault(),store.get("shogun.midimap")||{})}
function midiInit(ask){if(MIDI.access)return Promise.resolve(true);
  if(!navigator.requestMIDIAccess){if(ask)info.textContent="This browser has no Web MIDI";return Promise.resolve(false)}
  return navigator.requestMIDIAccess().then(a=>{MIDI.access=a;MIDI.ok=true;const hook=()=>a.inputs.forEach(i=>i.onmidimessage=midiMsg);hook();a.onstatechange=hook;return true},
    e=>{if(ask)info.textContent="MIDI is not available here ("+((e&&e.message)||"refused")+")";return false})}
const midiText=k=>MIDI.map[k]>=0?"MIDI "+MIDI.map[k]+" ("+noteName(MIDI.map[k])+")":"no MIDI note";
function midiMsg(e){const[st,n,vel]=e.data,t=st&0xf0;if(t!=0x90&&t!=0x80)return;const on=t==0x90&&vel>0;
  if(on&&MIDI.learn){for(const k in MIDI.map)if(MIDI.map[k]==n)MIDI.map[k]=-1;MIDI.map[sel]=n;store.set("shogun.midimap",MIDI.map);
    const k=sel;stepTrack(1);info.textContent=`${VK[k].t} plays from ${midiText(k)} · now hit a pad for ${VK[sel].t}, or LEARN to finish`}
  for(const k in MIDI.map)if(MIDI.map[k]==n){const v=VI.indexOf(k);audio();
    if(on)send(NOTEK[k]?[["sg_trigger_note",v,n,vel/127]]:[["sg_trigger",v,vel/127,0]]);else if(NOTEK[k])send([["sg_release",v]])}}
function learnPress(shift){if(shift){MIDI.map=midiDefault();store.set("shogun.midimap",MIDI.map);info.textContent="MIDI map back to notes 36 to 51";return}
  if(MIDI.learn){MIDI.learn=false;info.textContent="MIDI learn off";drawAll();return}
  midiInit(true).then(ok=>{if(!ok)return;MIDI.learn=true;drawAll();info.textContent=`MIDI learn: hit a pad for ${VK[sel].t}, or pick another track first · LEARN again to finish`})}

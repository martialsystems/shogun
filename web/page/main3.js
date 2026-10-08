
// ================= controls =================
let kdrag=null,pressed=null;
function ctrlAt(x,y){for(const c of CTRL){if(!zoneOn(c.zone))continue;if(c.rect?Math.abs(x-c.x)<=(c.rw||c.r)&&Math.abs(y-c.y)<=(c.rh||c.r):Math.hypot(x-c.x,y-c.y)<=c.r)return c}return null}
const NOFLAM={CP:1,LEAD:1,BASS:1};
function describe(c){const t=tracks[sel];
  if(c.kind=="knob"){const v=P[c.id];let s=c.name+" · "+(c.fmt?c.fmt(v):MAP[c.id]&&MAP[c.id].f?"CC "+ccOf(c.id,v):Math.round(v*100)+"%");
    if(c.id=="STEP:FLAM"&&NOFLAM[sel])s+=` (${VK[sel].t} has no flam)`;if(c.id=="STEP:BEND"&&!BENDK[sel])s+=` (${VK[sel].t} has no bend)`;if(c.id=="STEP:NOTE"&&!NOTEK[sel])s+=" (LEAD and BASS only)";
    if(/^(SEQ|STEP):/.test(c.id))s=VK[sel].t+" · "+s;if(/^STEP:/.test(c.id))s+=` · step ${edit+1}`;return s}
  if(c.id=="CLOCK:SOURCE")return P[c.id]>.5?"EXT · the pattern does not fire the voices; each drum fires from its Trig jack in the bay, and the count keeps running":"INT · the pattern fires the voices; the Trig jacks are ignored";
  if(c.id=="BAY")return bay?"BAY · press to close the bay; the cables stay patched":"BAY · press to open the patch bay under the sequencer ("+cables.length+" cable"+(cables.length==1?"":"s")+")";
  if(c.kind=="toggle")return c.name+" · "+(P[c.id]>.5?c.marks[1]:c.marks[0]);
  if(c.id=="TEST")return "TEST · play "+VK[sel].t+" once";
  if(c.voice)return "Select "+VK[c.voice].t+" · "+tracks[c.voice].len+" steps"+(tracks[c.voice].mute?" · muted":"")+" · "+midiText(c.voice);
  if(c.id=="START")return running?"STOP":"START";if(c.id=="CLEAR")return "CLEAR · empty the "+VK[sel].t+" track";
  if(c.id=="MUTE")return VK[sel].t+" · "+(t.mute?"muted":"playing");if(c.id=="TIE")return VK[sel].t+" · step "+(edit+1)+" · "+(t.steps[edit].tie?"tied to the step before":"not tied")+(NOTEK[sel]?"":" (LEAD and BASS only)");
  if(c.id.startsWith("PAGE:"))return "Steps "+(c.id=="PAGE:0"?"1 to 16":"17 to 32");
  if(c.step!=null){const n=page*16+c.step,s=t.steps[n];if(n>=t.len)return `${VK[sel].t} · step ${n+1} · past the track length (${t.len})`;
    return `${VK[sel].t} · step ${n+1} · `+(s.on?(NOTEK[sel]?noteName(s.note)+(s.tie?" tied":""):["soft","medium","loud"][s.acc]):"off")+(c.kind=="acc"?" · click for soft, medium, loud":"")}
  if(c.id=="SOLO")return soloV?`SOLO · only ${VK[soloV].t} is heard`+(tracks[soloV].mute?" (muted, so nothing)":"")+(soloV==sel?" · press to hear every track":` · press to solo ${VK[sel].t}`):`SOLO · hear only ${VK[sel].t} (not saved with the pattern)`;
  if(c.id=="UNDO")return "UNDO the last knob or key ("+UNDO.length+") · Shift-click or Ctrl+Shift+Z redoes ("+REDO.length+")";
  if(c.id=="RANDOM")return "RANDOM · roll the "+VK[sel].t+" knobs inside their ranges (LEVEL stays)";
  if(c.id=="LEARN")return "MIDI LEARN · then pick a track and hit a pad · Shift-click resets to notes 36 to 51";
  if(c.id=="KIT")return "Kit "+curKit.n+" · click for the kit list; a kit changes the knobs and keeps the steps";
  if(c.id=="KSAVE")return "Save the knobs as a kit (type a name)";
  if(c.id=="CHN")return "CHAIN "+(CHAIN.on?"on":"off")+" · click to turn it "+(CHAIN.on?"off":"on")+(CHAIN.list.length?" · "+CHAIN.list.map(chainName).join(", "):" · + adds the loaded pattern");
  if(c.id=="CH:ADD")return "Add "+chainName(curPat)+" to the chain";
  if(c.id=="CH:DEL")return "Remove the last pattern from the chain · Shift-click clears it";
  if(c.id=="COPY")return "Copy this pattern under a new name";
  return {PREV:"Previous pattern",NEXT:"Next pattern",SAVE:DD.naming=="PAT"?"Save as "+(DD.name.trim()||"PATTERN"):userIdx()>=0?"Save over "+chainName(curPat):"Save this pattern and its knobs under a name",
    SCR:"Pattern list",TRK:"Track list",TPREV:"Previous track",TNEXT:"Next track"}[c.id]||c.id}
function setP(id,v){v=clamp(v);if(STEPS[id]){const n=STEPS[id]-1;v=Math.round(v*n)/n}P[id]=v;syncLinked(id);const t=tracks[sel],st=t.steps[edit];
  if(MAP[id]){sendParam(id);if(!MAP[id].master){dirty=true;curKit.dirty=true}}
  else if(id.startsWith("CLOCK:")){sendClock();dirty=true}
  else if(id.startsWith("LFO:"))sendLfo();
  else if(id=="SEQ:LENGTH"){t.len=1+Math.round(v*31);if(edit>=t.len)edit=t.len-1;dirty=true;sendTrack(sel)}
  else if(id=="SEQ:SHUFFLE"){t.shuffle=Math.round(v*15);dirty=true;sendTrack(sel)}
  else if(id=="SEQ:SHIFT"){t.shift=Math.round(v*127);dirty=true;sendTrack(sel)}
  else if(id=="STEP:FLAM"){st.flam=Math.round(v*16)-1;dirty=true;sendSteps(sel)}
  else if(id=="STEP:BEND"){st.bend=Math.abs(v-.5)<.006?null:Math.round(v*127);dirty=true;sendSteps(sel)}
  else if(id=="STEP:NOTE"){st.note=36+Math.round(v*36);dirty=true;sendSteps(sel)}
  if(/:MODE$/.test(id))dirty=true;drawAll()}
// the index of the loaded pattern among the saved ones (-1 for a factory pattern)
const userIdx=()=>curPat.i-(curPat.b=="A"?FACT.length:0);
const UNDOABLE={toggle:1,acc:1};
function press(c,e){const t=tracks[sel];
  if(c.kind=="knob"||UNDOABLE[c.kind]||c.step!=null||/^(CLEAR|MUTE|TIE)$/.test(c.id))undoPush(c.kind=="knob"?c.id:null);
  if(c.kind=="knob"){kdrag={c,y:e.clientY,v:P[c.id]};sv.setPointerCapture(e.pointerId);return}
  if(c.kind=="toggle")setP(c.id,P[c.id]>.5?0:1);
  else if(c.voice){selTrack(c.voice)}
  else if(c.id=="TEST")audition(sel);
  else if(c.id=="START")setRun(!running);
  else if(c.id=="CLEAR"){t.steps=Array.from({length:32},blankStep);dirty=true;sendSteps(sel);loadKnobs()}
  else if(c.id.startsWith("PAGE:"))page=+c.id.slice(5);
  else if(c.id=="MUTE"){t.mute=!t.mute;dirty=true;sendTrack(sel)}
  else if(c.id=="TIE"){const st=t.steps[edit];st.tie=!st.tie;if(st.tie)st.on=true;dirty=true;sendSteps(sel)}
  else if(c.id=="PREV"||c.id=="NEXT"){loadPat(bankView,(curPat.b==bankView?curPat.i:c.id=="NEXT"?-1:0)+(c.id=="NEXT"?1:-1));return}
  else if(c.id=="SAVE"){if(DD.naming=="PAT")nameCommit();else if(userIdx()>=0){const p=patList(curPat.b)[curPat.i];savePat(p.n,userIdx())}else nameStart("PAT");return}
  else if(c.id=="COPY"){const p=patList(curPat.b)[curPat.i];nameStart("PAT",p?p.n:"");return}
  else if(c.id=="KSAVE"){if(DD.naming=="KIT")nameCommit();else nameStart("KIT",curKit.n=="INIT"?"":curKit.n);return}
  else if(c.id=="UNDO"){undo(e.shiftKey);pressed=c.id;drawAll();return}
  else if(c.id=="BAY")setBay(!bay);
  else if(c.id=="SOLO"){soloV=soloV==sel?null:sel;send([["sg_set_solo",soloV?VI.indexOf(soloV):-1]])}
  else if(c.id=="RANDOM"){randomVoice();pressed=c.id;drawAll();return}
  else if(c.id=="LEARN"){learnPress(e.shiftKey);return}
  else if(c.id=="CHN"){if(!CHAIN.list.length){info.textContent="The chain is empty · + adds the loaded pattern";return}CHAIN.on=!CHAIN.on;chainPos=-1;saveChain()}
  else if(c.id=="CH:ADD"){if(CHAIN.list.length<64)CHAIN.list.push({b:curPat.b,i:curPat.i});saveChain();info.textContent="Chain: "+CHAIN.list.map(chainName).join(", ");drawAll();return}
  else if(c.id=="CH:DEL"){if(e.shiftKey)CHAIN.list=[];else CHAIN.list.pop();if(!CHAIN.list.length)CHAIN.on=false;saveChain();info.textContent=CHAIN.list.length?"Chain: "+CHAIN.list.map(chainName).join(", "):"The chain is empty";drawAll();return}
  else if(c.id=="SCR"||c.id=="TRK"||c.id=="KIT"){const k=c.id=="SCR"?"PAT":c.id;if(DD.open==k)ddClose();else ddOpen(k);return}
  else if(c.id=="TPREV"||c.id=="TNEXT"){stepTrack(c.id=="TNEXT"?1:-1);return}
  else if(c.step!=null){const n=page*16+c.step,st=t.steps[n];edit=n;
    if(c.kind=="acc"||e.shiftKey||e.button==2){st.acc=st.on?(st.acc+1)%3:2;st.on=true}else{st.on=!st.on;if(!st.on)st.tie=false}
    dirty=true;sendSteps(sel);loadKnobs()}
  pressed=c.kind=="key"?c.id:null;drawAll();info.textContent=describe(c)}
sv.addEventListener("contextmenu",e=>{pt(e);if(ctrlAt(mx,my))e.preventDefault()});
sv.addEventListener("wheel",e=>{pt(e);if(DD.open&&ddRowAt(mx,my)!==null){e.preventDefault();ddScroll(Math.sign(e.deltaY));return}const c=ctrlAt(mx,my);if(c&&c.kind=="knob"){e.preventDefault();undoPush(c.id);setP(c.id,P[c.id]-Math.sign(e.deltaY)*(STEPS[c.id]?1/(STEPS[c.id]-1):e.shiftKey?.005:.025));info.textContent=describe(c)}},{passive:false});
sv.addEventListener("dblclick",e=>{pt(e);const c=ctrlAt(mx,my);if(c&&c.kind=="knob"&&DEF[c.id]!=null){undoPush(null);setP(c.id,DEF[c.id]);info.textContent=describe(c)}});
sv.addEventListener("pointermove",e=>{pt(e);
  if(kdrag){const c=kdrag.c;kdrag.v=clamp(kdrag.v+(kdrag.y-e.clientY)/(e.shiftKey?900:180));kdrag.y=e.clientY;setP(c.id,kdrag.v);info.textContent=describe(c);return}
  ptrIn=true;
  // over an open list: the row under the pointer is the highlighted one, and nothing under the list reacts
  if(DD.open){const r=ddRowAt(mx,my);if(r!==null){setHov(null);sv.style.cursor=r?"pointer":"default";if(r){const k=ddItems().findIndex(x=>x.i==r.i);if(k!=DD.hi){DD.hi=k;ddDraw()}}return}}
  const h=near();setHov(h);const c=h?null:ctrlAt(mx,my);if(c&&!grab)info.textContent=describe(c);
  sv.style.cursor=c?(c.kind=="knob"?"ns-resize":"pointer"):(h||grab?"grab":"default");
  if(down&&!grab&&Math.hypot(mx-down.x,my-down.y)>6){down.moved=true;const pl=plugsAt(down.j);if(pl.length&&!down.shift){const p=pl[pl.length-1];startGrab(p.i,p.e,false)}else newCable(down.j,false)}});
sv.addEventListener("pointerleave",()=>{ptrIn=false;if(!grab)setHov(null)});
sv.addEventListener("pointerdown",e=>{pt(e);hideMenu();audio();
  if(grab&&grab.carry){drop();ignoreUp=true;return}
  if(DD.open){const r=ddRowAt(mx,my);if(r!==null){if(r)ddChoose(r);return}}
  const h=near();if(h){down={j:h,x:mx,y:my,moved:false,shift:e.shiftKey};sv.setPointerCapture(e.pointerId);return}
  const c=ctrlAt(mx,my);
  // an open list takes the click: a row loads, anywhere else closes it (its own screen toggles it)
  if(DD.open){const r=ddRowAt(mx,my);if(r!==null){if(r)ddChoose(r);return}if(!c||c.kind!="screen")ddClose()}
  if(DD.naming&&!(c&&(c.id=="SAVE"||c.id=="KSAVE"))){nameCancel();info.textContent="Save cancelled"}
  if(c)press(c,e)});
sv.addEventListener("pointerup",e=>{pt(e);if(pressed){pressed=null;drawAll()}
  if(kdrag){kdrag=null;return}
  if(ignoreUp){ignoreUp=false;down=null;return}
  if(grab&&!grab.carry){drop();down=null;return}
  if(down&&!down.moved&&plugsAt(down.j).length)showMenu(down.j);down=null});
sv.addEventListener("pointercancel",()=>{kdrag=null;down=null;pressed=null});
addEventListener("keydown",e=>{if(ddKey(e))return;
  if((e.ctrlKey||e.metaKey)&&!/^(INPUT|TEXTAREA)$/.test(document.activeElement.tagName)){const k=e.key.toLowerCase();if(k=="z"||k=="y"){e.preventDefault();undo(k=="y"||e.shiftKey);return}}if(e.key=="Escape"){hideMenu();cancel()}
  if(e.code=="Space"&&!/^(BUTTON|INPUT)$/.test(document.activeElement.tagName)){e.preventDefault();setRun(!running)}});
$("go").onclick=()=>setRun(!running);
$("tabs").onclick=e=>{const b=e.target.closest(".tab");if(b&&b.dataset.v!=view)setView(b.dataset.v)};

// ================= start =================
function loop(){if(grab&&cables[grab.i]&&cables[grab.i].el)pins(cables[grab.i]);step();paint();drawAll();requestAnimationFrame(loop)}
loadLfo();showZones();loadUser();USERK=store.get("shogun.kits")||[];loadChain();midiLoad();loadPat("A",0);
try{navigator.permissions.query({name:"midi"}).then(r=>{if(r.state=="granted")midiInit(false)},()=>{})}catch(e){}drawAll();build();requestAnimationFrame(loop);
window.SHOGUN={P,get tracks(){return tracks},get cables(){return cables},JACKS,CTRL,setRun,get running(){return running},get counter(){return counter},patList,loadPat,savePat,get cur(){return curPat},get curKit(){return curKit},kitList,loadKit,saveKit,CHAIN,MIDI,midiMsg,undo,get undoDepth(){return UNDO.length},randomVoice,get rot0(){return rot0},get solo(){return soloV},get bankView(){return bankView},DD,ddOpen,ddKey,legal,get engine(){return node||host},send,press,setP,get sel(){return sel},get bay(){return bay},setBay,get view(){return view},setView,get lfoV(){return lfoV}};

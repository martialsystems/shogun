// ================= geometry =================
// One editor. The top bar, the knob block and the sequencer are always drawn. The BAY key adds the patch bay and its
// cable hang below the sequencer, and the panel grows to hold them.
const NS="http://www.w3.org/2000/svg",W=1600,M=14,AV=W-2*M,FR_T=68,BAND=164,INK="#f4f3ee",GRN="#4aa862",JR=9;
// BAY off: the panel ends under the sequencer (H_SEQ). BAY on: the bay (two jack rows) and its hang are added below it and
// the panel grows to H. The LFO and VOICE tabs replace the face with their own page (H_LFO, H_VOICE) under the same top bar.
const KB_T=FR_T,KB_B=KB_T+2*BAND,SEQ_H=146,SEQ_B=KB_B+SEQ_H,BAY_T=SEQ_B,BAY_B=BAY_T+92,BAY2_B=BAY_B+78,FR_B=BAY2_B+110,H=FR_B+14,H_SEQ=SEQ_B+14;
const H_LFO=FR_T+262+14,H_VOICE=FR_T+262+14;
let VH=H_SEQ;   // the height in view now
const $=id=>document.getElementById(id),sv=$("sv"),cab=$("cab"),ring=$("ring"),info=$("info"),menu=$("menu"),stage=$("stage");
sv.setAttribute("viewBox",`0 0 ${W} ${VH}`);
const clamp=(v,a=0,b=1)=>Math.max(a,Math.min(b,v));
// Panel defaults: the example values printed in SCHEMATICS.md, by engine knob name (shogun::Knobs).
const KD={bd1Attack:64,bd1Decay:80,bd1Pitch:40,bd1Tune:50,bd1Noise:0,bd1Filter:64,bd1Dist:0,bd1Trigger:0,bd2Tune:60,bd2Decay:70,bd2Tone:100,
  sdTune:70,sdDTune:90,sdPitch:30,sdTone:64,sdToneDecay:60,sdSnappy:70,sdSnDecay:50,rsTune:40,cpAttack:100,cpDecay:50,cpFilter:64,cpTrigger:0,cpData:48,
  clTune:50,clDecay:30,cyTune:64,cyTone:70,cyDecay:90,ohDecay:100,hhTune:60,hhDecay:40,ltcTune:40,ltcDecay:60,mtcTune:50,mtcDecay:60,htcTune:60,htcDecay:60,
  tomNoise:33,bd1Wave:32,bd2Wave:32,ltcWave:32,mtcWave:32,htcWave:32,cbTune:48,cbDecay:70,maDecay:55,leadTone:80,bassTone:64};
const VI=["BD1","BD2","SD","RS","CY","OH","HH","CL","CP","LTC","MTC","HTC","CB","MA","LEAD","BASS"];   // shogun::Voice order

// ================= the instrument =================
// knobs: [label, engine param, positions, id] (positions = a stepped control; "tog" = an OFF/ON switch; id defaults to the
// label and keeps the names saved patterns use). LEVEL is the linear level. Only knobs the engine has are drawn.
// WAVE is the folder on the body (0 is bypass). hide: engine knobs with no knob on this face (a kit still sets them; the
// VOICE tab has a knob for each).
const VOICES=[
 {k:"BD1",t:"BD 1",b:0,knobs:[["TUNE","bd1Tune"],["BEND","bd1Pitch",0,"PITCH"],["DECAY","bd1Decay"],["ATTACK","bd1Attack"],["DIST","bd1Dist"],["NOISE","bd1Noise"],["FILTER","bd1Filter"],["WAVE","bd1Wave"],["LEVEL"]],hide:[["SOUND","bd1Trigger",16]]},
 {k:"BD2",t:"BD 2",b:0,knobs:[["TUNE","bd2Tune"],["DECAY","bd2Decay"],["TONE","bd2Tone"],["WAVE","bd2Wave"],["LEVEL"]]},
 {k:"SD",t:"SNARE",b:0,knobs:[["TUNE","sdTune"],["DETUNE","sdDTune",0,"D-TUNE"],["BEND","sdPitch",0,"PITCH"],["DECAY","sdToneDecay",0,"T.DECAY"],["DEC 2","sdSnDecay",0,"SN.DECAY"],["SNAPPY","sdSnappy"],["TONE","sdTone"],["LEVEL"]]},
 {k:"RS",t:"RIM",b:0,knobs:[["TUNE","rsTune"],["LEVEL"]]},
 {k:"CP",t:"CLAP",b:0,knobs:[["BURSTS","cpData",8,"COUNT"],["ATTACK","cpAttack"],["DECAY","cpDecay"],["FILTER","cpFilter"],["LEVEL"]],hide:[["SOUND","cpTrigger",16]]},
 {k:"CL",t:"CLAVES",b:0,knobs:[["TUNE","clTune"],["DECAY","clDecay"],["LEVEL"]]},
 {k:"MA",t:"MARACAS",b:0,knobs:[["DECAY","maDecay"],["LEVEL"]]},
 {k:"CB",t:"COWBELL",b:1,knobs:[["TUNE","cbTune"],["DECAY","cbDecay"],["LEVEL"]]},
 {k:"CY",t:"CYMBAL",b:1,knobs:[["TUNE","cyTune"],["TONE","cyTone"],["DECAY","cyDecay"],["LEVEL"]]},
 {k:"OH",t:"OPEN HAT",b:1,knobs:[["TUNE","hhTune"],["DECAY","ohDecay"],["LEVEL"]]},
 {k:"HH",t:"CLOSED HAT",b:1,knobs:[["TUNE","hhTune"],["DECAY","hhDecay"],["LEVEL"]]},
 {k:"LTC",t:"LOW TOM",b:1,tog:1,knobs:[["TUNE","ltcTune"],["DECAY","ltcDecay"],["NOISE","ltcNoise","tog"],["WAVE","ltcWave"],["LEVEL"]]},
 {k:"MTC",t:"MID TOM",b:1,tog:1,knobs:[["TUNE","mtcTune"],["DECAY","mtcDecay"],["NOISE","mtcNoise","tog"],["WAVE","mtcWave"],["LEVEL"]]},
 {k:"HTC",t:"HI TOM",b:1,tog:1,knobs:[["TUNE","htcTune"],["DECAY","htcDecay"],["NOISE","htcNoise","tog"],["WAVE","htcWave"],["LEVEL"]]},
 {k:"LEAD",t:"LEAD",b:1,note:1,knobs:[["TONE","leadTone"],["LEVEL"]]},
 {k:"BASS",t:"BASS",b:1,note:1,knobs:[["TONE","bassTone"],["LEVEL"]]}];
const VK=Object.fromEntries(VOICES.map(v=>[v.k,v])),NOTEK={LEAD:1,BASS:1},BENDK={BD1:1,BD2:1,SD:1,LTC:1,MTC:1,HTC:1};
const P={},DEF={},CTRL=[],JACKS={},LIVE={},MAP={},ALIAS={};   // panel values 0..1, defaults, hit regions, jacks, live svg groups, panel id -> engine target, alias -> panel id
// Static art and live parts go to a zone, shown or hidden as a whole: top (the top bar), face, seq, baykey (the RACK tab),
// bay (the RACK tab with BAY on), lfo, voice, and v_<voice> (the VOICE tab with that voice picked). Z is the zone being drawn.
const ART={},LZ={};let Z,S,AL=false;   // AL: knobs and switches being drawn are aliases of face controls (the VOICE tab)
const zone=z=>{Z=z;S=ART[z]=ART[z]||[]};zone("face");
const ctl=o=>{o.zone=Z;CTRL.push(o);return o};
const T=(x,y,s,z=11,anchor="middle",fill=INK,ls=.5)=>S.push(`<text x="${x.toFixed(1)}" y="${y.toFixed(1)}" font-size="${z}" font-weight="700" letter-spacing="${ls}" text-anchor="${anchor}" fill="${fill}">${s}</text>`);
const live=(id,x=0,y=0)=>{LIVE[id]={x,y};LZ[id]=Z;S.push(`<g id="L_${id.replace(/[^A-Za-z0-9]/g,"_")}"></g>`)};
const lv=id=>document.getElementById("L_"+id.replace(/[^A-Za-z0-9]/g,"_"));
const rule=(x1,y1,x2,y2,w=1.6)=>S.push(`<line x1="${x1.toFixed(1)}" y1="${y1}" x2="${x2.toFixed(1)}" y2="${y2}" stroke="${GRN}" stroke-width="${w}"/>`);
function ticks(cx,cy,r,n=11){for(let i=0;i<n;i++){const a=(-135+270*i/(n-1)-90)*Math.PI/180,l=i==0||i==n-1||i==(n-1)/2?4:2.5;
  S.push(`<line x1="${(cx+(r+4)*Math.cos(a)).toFixed(1)}" y1="${(cy+(r+4)*Math.sin(a)).toFixed(1)}" x2="${(cx+(r+4+l)*Math.cos(a)).toFixed(1)}" y2="${(cy+(r+4+l)*Math.sin(a)).toFixed(1)}" stroke="${INK}" stroke-width="1.1"/>`)}}
const STEPS={};   // stepped controls: id -> positions
// An alias (AL) draws a second knob for the same panel value: it sets nothing, and its hit region moves the real control.
function knob(id,label,cx,cy,def,opt={}){const r=opt.r||14,lid=AL?id+"@"+Z:id;if(AL)ALIAS[lid]=id;else{P[id]=DEF[id]=def;if(opt.n)STEPS[id]=opt.n}
  const n=opt.n||STEPS[id];ticks(cx,cy,r,n&&n<=16?n:11);live(lid);LIVE[lid]={x:cx,y:cy,r};
  if(opt.marks)opt.marks.forEach((m,i)=>{const a=(-135+270*i/(opt.marks.length-1)-90)*Math.PI/180;T(cx+(r+16)*Math.cos(a),cy+(r+16)*Math.sin(a)+3,m,8.5)});
  if(label)T(cx,cy+r+(opt.ly||19),label,opt.lz||10);ctl({id,kind:"knob",x:cx,y:cy,r:r+8,name:opt.name||id,fmt:opt.fmt})}
function key(id,cx,cy,label,lamp){S.push(`<rect x="${cx-17}" y="${cy-17}" width="34" height="34" rx="4" fill="#050505" stroke="#262826" stroke-width="1.2"/>`);live(id);LIVE[id]={x:cx,y:cy};
  if(label)T(cx,cy+30,label,10);if(lamp)ledAt(lamp,lamp.x,lamp.y);return ctl({id,kind:"key",x:cx,y:cy,r:17,rect:1})}
function ledAt(l,cx,cy,r=4.5){S.push(`<circle cx="${cx}" cy="${cy}" r="${r+2}" fill="#050505" stroke="#262826"/>`);live(l.id);LIVE[l.id]={x:cx,y:cy,r}}
// A jack is drawn on the bay page only. Its name under it is part of its hover area (lx, ly).
function jack(id,label,cx,cy,dir,type,name){S.push(`<circle cx="${cx}" cy="${cy}" r="${JR+1.5}" fill="#000" opacity=".55"/><circle cx="${cx}" cy="${cy}" r="${JR}" fill="url(#js)" stroke="#2a2a2c" stroke-width=".9"/><circle cx="${cx}" cy="${cy}" r="${JR-2.6}" fill="url(#jn)"/><circle cx="${cx}" cy="${cy}" r="${JR-5}" fill="#030303"/>`);
  T(cx,cy+24.5,label,9.5);JACKS["SHOGUN/"+id]={x:cx,y:cy,lx:cx,ly:cy+21,lw:Math.max(14,label.length*3.6),dir,type,name}}
function toggle(id,cx,cy,marks,def,name){const lid=AL?id+"@"+Z:id;if(AL)ALIAS[lid]=id;else P[id]=DEF[id]=def;S.push(`<rect x="${cx-15}" y="${cy-8}" width="30" height="16" rx="3" fill="#050505" stroke="#262826"/><rect x="${cx-10}" y="${cy-2}" width="20" height="4" rx="2" fill="#000"/><circle cx="${cx}" cy="${cy}" r="5" fill="url(#jn)" stroke="#111" stroke-width=".8"/>`);
  T(cx-21,cy+4,marks[0],9,"end");T(cx+21,cy+4,marks[1],9,"start");live(lid);LIVE[lid]={x:cx,y:cy};ctl({id,kind:"toggle",x:cx,y:cy,r:18,marks,name:name||id})}
const lcdBox=(x,y,w,h)=>S.push(`<rect x="${x}" y="${y}" width="${w}" height="${h}" rx="3" fill="#0a0a0b" stroke="#2a2a2c" stroke-width="1.2"/><rect x="${x+4}" y="${y+4}" width="${w-8}" height="${h-8}" rx="1.5" fill="url(#lcd)"/>`);
const LCD={};function lcd(id,x,y,w,label,chars){lcdBox(x,y,w,28);LCD[id]={x:x+6,y:y+6,w:w-12,h:16,n:chars};live("LCD:"+id);if(label)T(x+w/2,y+43,label,10)}
const ccOf=(id,v)=>{const n=STEPS[id];if(!n)return Math.round(v*127);const i=Math.round(v*(n-1));return Math.floor(i*128/n)};   // a stepped control sends the lowest controller value of its position
const vOf=(id,cc,n)=>n?Math.floor(cc*n/128)/(n-1):cc/127;

// ---- the knob block: two bands of voice strips. Each strip has its name and lamp on top (click the name to select the
// track; TEST in the top bar plays it) and its knobs in two rows.
const KCOL=64,PAD=12,NW=t=>t.length*9.4;
const BANDS=[VOICES.filter(v=>v.b==0),VOICES.filter(v=>v.b==1)];
BANDS[1].splice(BANDS[1].findIndex(s=>s.k=="LEAD"),0,{k:"TOMS",t:"TOMS",knobs:[0]});BANDS[1].push({k:"MASTER",t:"MASTER",knobs:[0]});
const colsOf=s=>Math.ceil(s.knobs.length/2);
BANDS.forEach((band,bi)=>{const minW=s=>Math.max(colsOf(s)*KCOL+PAD,NW(s.t)+(s.tog?130:30)),sc=AV/band.reduce((a,s)=>a+minW(s),0);let x=M;
  band.forEach((s,i)=>{s.x=x;s.w=minW(s)*sc;s.y=KB_T+bi*BAND;x+=s.w;if(i<band.length-1)rule(x,s.y,x,s.y+BAND)})});
rule(M,KB_T+BAND,W-M,KB_T+BAND);
S.push(`<line x1="${M}" y1="${KB_B}" x2="${W-M}" y2="${KB_B}" stroke="${GRN}" stroke-width="3.4"/>`);   // the heavier rule between the knob block and the sequencer
const HDR=21,KR=[60,126],KNR=15;   // strip header baseline; the two knob rows; knob radius
// the strip name, with the track lamp after it; tom strips put the name left to make room for TOM/CGA
function header(s,lamp){const nw=NW(s.t),nx=s.tog?s.x+12+nw/2:s.x+s.w/2-(lamp?9:0);T(nx,s.y+HDR,s.t,12,"middle",INK,1);
  if(lamp)ledAt({id:"LED:"+s.k},nx+nw/2+10,s.y+HDR-4,4)}
function voiceSection(v){const{x,w,y}=v,cols=colsOf(v),cw=w/cols;header(v,1);
  if(v.tog){toggle(v.k+":MODE",x+w-46,y+HDR-4,["TOM","CGA"],0,v.t+" · TOM or CONGA");MAP[v.k+":MODE"]={f:v.k.toLowerCase()+"Mode",tog:1}}
  v.knobs.forEach(([lb,p,n,idn],i)=>{const kx=x+cw*(i%cols+.5),ky=y+KR[Math.floor(i/cols)],id=v.k+":"+(idn||lb);
    if(n=="tog"){toggle(id,kx,ky,["",""],0,v.t+" · NOISE");T(kx-9,ky-13,"OFF",7.5);T(kx+10,ky-13,"ON",7.5);T(kx,ky+32,lb,9.5);MAP[id]={f:p,tog:1};return}
    if(!p){knob(id,lb,kx,ky,1,{r:KNR,lz:9.5,ly:17,name:v.t+" · LEVEL"});MAP[id]={level:VI.indexOf(v.k)};return}
    knob(id,lb,kx,ky,vOf(id,KD[p],n||0),{r:KNR,lz:9.5,ly:17,n:n||0,name:v.t+" · "+lb+(p=="hhTune"?" (one tune for the open and closed hats)":/Wave$/.test(p)?" (the folder on the body; 0 is bypass)":""),fmt:n==8?x=>(1+Math.round(x*7))+" bursts":null});MAP[id]={f:p}});
  (v.hide||[]).forEach(([lb,p,n])=>{const id=v.k+":"+lb;P[id]=DEF[id]=vOf(id,KD[p],n);STEPS[id]=n;MAP[id]={f:p}});
  // the name selects the track (pushed last, so the TOM/CGA switch in the same header wins its own clicks)
  ctl({id:"SEL:"+v.k,kind:"sel",voice:v.k,x:v.tog?x+(w-84)/2:x+w/2,y:y+HDR-5,rw:v.tog?(w-84)/2:w/2-4,rh:13,rect:1})}
VOICES.forEach(voiceSection);
const sec=k=>BANDS.flat().find(s=>s.k==k);
{const s=sec("TOMS"),cx=s.x+s.w/2;header(s,0);
 knob("TOMS:NOISE","NOISE LVL",cx,s.y+KR[0],KD.tomNoise/127,{r:KNR,lz:9.5,ly:17,name:"TOMS · NOISE level, shared by the three"});MAP["TOMS:NOISE"]={f:"tomNoise"}}
{const s=sec("MASTER"),cx=s.x+s.w/2;header(s,0);
 knob("OUT:MASTER","VOLUME",cx,s.y+KR[0],.65,{r:KNR,lz:9.5,ly:17,name:"MASTER · VOLUME (scales the mix)"});MAP["OUT:MASTER"]={master:1}}
// knobs that share one engine control (the hat tune) move together
const LINKED={};for(const id in MAP){const f=MAP[id].f;if(f)(LINKED[f]=LINKED[f]||[]).push(id)}
function syncLinked(id){const m=MAP[id];if(m&&m.f)LINKED[m.f].forEach(o=>{P[o]=P[id]})}

// ---- the sequencer page (BAY off): CLOCK, TRACK, STEP, and the 16 step keys
zone("seq");
const bpmOf=v=>60+v*120,SCALEN=["32ND","16T","16TH","8T"];
const CKW=318,TKW=300,STW=272,SX=M+CKW+TKW+STW,SW=(W-M-SX)/16,y3=KB_B,L1=y3+48,L2=y3+108;
[M+CKW,M+CKW+TKW,SX].forEach(x=>rule(x,y3,x,SEQ_B));
{const x=M;T(x+CKW/2,y3+16,"CLOCK",12.5,"middle",INK,1);
 knob("CLOCK:TEMPO","TEMPO",x+40,L1,.5,{name:"TEMPO",fmt:v=>bpmOf(v).toFixed(1)+" BPM"});lcd("BPM",x+72,L1-14,66,"BPM",3);ctl({id:"CLOCK:TEMPO",kind:"knob",x:x+105,y:L1,r:30,name:"TEMPO",fmt:v=>bpmOf(v).toFixed(1)+" BPM"});
 toggle("CLOCK:SOURCE",x+236,L1,["INT","EXT"],0,"CLOCK");T(x+236,L1+33,"TRIG SOURCE",9);
 knob("CLOCK:SCALE","SCALE",x+46,L2,2/3,{n:4,marks:["32","16T","16","8T"],name:"SCALE",fmt:v=>["32nd notes, 8 steps per beat","16th triplets, 6 steps per beat","16th notes, 4 steps per beat","8th triplets, 3 steps per beat"][Math.round(v*3)]});
 knob("CLOCK:BAR","BAR",x+136,L2,15/31,{n:32,name:"BAR",fmt:v=>(1+Math.round(v*31))+" steps per bar"});lcd("POS",x+196,L2-14,56,"STEP",2)}
{const x=M+CKW;T(x+TKW/2,y3+16,"TRACK",12.5,"middle",INK,1);
 knob("SEQ:LENGTH","LENGTH",x+40,L1,15/31,{n:32,name:"LENGTH",fmt:v=>(1+Math.round(v*31))+" steps"});lcd("LEN",x+70,L1-14,62,"STEPS",2);ctl({id:"SEQ:LENGTH",kind:"knob",x:x+101,y:L1,r:30,name:"LENGTH",fmt:v=>(1+Math.round(v*31))+" steps"});
 knob("SEQ:SHUFFLE","SHUFFLE",x+182,L1,0,{n:16,name:"SHUFFLE",fmt:v=>"intensity "+Math.round(v*15)+" of 15"});
 knob("SEQ:SHIFT","SHIFT",x+250,L1,0,{name:"SHIFT",fmt:v=>(Math.round(v*127)/127*30).toFixed(1)+" ms later"});
 key("PAGE:0",x+36,L2,"1-16",{id:"LED:P0",x:x+60,y:L2-12});key("PAGE:1",x+106,L2,"17-32",{id:"LED:P1",x:x+130,y:L2-12});
 key("MUTE",x+180,L2,"MUTE",{id:"LED:MUTE",x:x+204,y:L2-12});key("CLEAR",x+256,L2,"CLEAR")}
{const x=M+CKW+TKW;T(x+STW/2,y3+16,"STEP",12.5,"middle",INK,1);lcd("EDIT",x+14,L1-14,58,"EDIT",2);
 knob("STEP:FLAM","FLAM",x+108,L1,0,{n:17,name:"FLAM",fmt:v=>{const i=Math.round(v*16);return i?"flam "+i+" of 16":"no flam"}});
 knob("STEP:BEND","BEND",x+170,L1,.5,{name:"BEND",fmt:v=>Math.abs(v-.5)<.006?"no bend":((12*(2*Math.round(v*127)/127-1))>=0?"+":"")+(12*(2*Math.round(v*127)/127-1)).toFixed(2)+" semitones"});
 knob("STEP:NOTE","NOTE",x+232,L1,24/36,{n:37,name:"NOTE",fmt:v=>noteName(36+Math.round(v*36))});
 key("TIE",x+32,L2,"TIE",{id:"LED:TIE",x:x+56,y:L2-12});key("UNDO",x+110,L2,"UNDO");key("RANDOM",x+186,L2,"RANDOM")}
T(SX-8,y3+104,"ACC",9,"end");
for(let i=0;i<16;i++){const cx=SX+SW*(i+.5);live("NUM:"+i);LIVE["NUM:"+i]={x:cx,y:y3+18};
  S.push(`<circle cx="${cx}" cy="${y3+34}" r="9" fill="url(#js)" stroke="#2a2a2c" stroke-width=".9"/><circle cx="${cx}" cy="${y3+34}" r="6.5" fill="#050505"/>`);live("PH:"+i);LIVE["PH:"+i]={x:cx,y:y3+34};
  const c=key("STEP:"+i,cx,y3+68);c.step=i;ledAt({id:"ACC:"+i},cx,y3+100,4);ctl({id:"ACC:"+i,kind:"acc",x:cx,y:y3+100,r:10,step:i});live("LK:"+i);LIVE["LK:"+i]={x:cx,y:y3+124};
  if(i%4==0&&i)rule(SX+SW*i,y3,SX+SW*i,SEQ_B)}
const NN=["C","C#","D","D#","E","F","F#","G","G#","A","A#","B"],noteName=n=>NN[n%12]+(Math.floor(n/12)-1);

// ---- the bay (BAY on), below the sequencer. Row 1: clock jacks at the left, then Trig and Out for each drum, then the mix.
// Row 2: the rack LFO's output and the CV inputs. Below the jack rows is the hang: no controls, only cables.
zone("bay");
const BAYV=[["BD1","BD1"],["BD2","BD2"],["SD","SD"],["RS","RS"],["CP","CP"],["CL","CL"],["MA","MA"],["CB","CB"],["CY","CY"],["OH","OH"],["HH","HH"],["LTC","LTC"],["MTC","MTC"],["HTC","HTC"]];
{const G=[{k:"CLOCK",n:5,u:54}].concat(BAYV.map(([k])=>({k,n:2,u:38})),[{k:"MASTER",n:2,u:44}]),sc=AV/G.reduce((a,g)=>a+g.n*g.u+8,0),jy=BAY_T+50;let x=M;
 G.forEach((g,gi)=>{const w=(g.n*g.u+8)*sc,cx=x+w/2,u=(w-8*sc)/g.n,j0=x+4*sc+u/2;T(cx,BAY_T+20,g.k,11.5,"middle",INK,1);
  if(g.k=="CLOCK")[["CLK IN","in","CLK IN"],["RST IN","in","RST IN"],["RUN IN","in","RUN IN"],["CLK OUT","out","CLK OUT"],["ACC OUT","out","ACC OUT"]].forEach(([lb,d,nm],i)=>jack("CLOCK:"+lb,lb,j0+u*i,jy,d,"Gate",nm));
  else if(g.k=="MASTER")[["L","left"],["R","right"]].forEach(([c],i)=>jack("MIX "+c,"MIX "+c,j0+u*i,jy,"out","Audio","Mix "+c));
  else{jack(g.k+":TRIG","TRIG",j0,jy,"in","Gate",g.k+" Trig");jack(g.k+":OUT","OUT",j0+u,jy,"out","Audio",g.k+" Out")}
  x+=w;if(gi<G.length-1)rule(x,BAY_T,x,BAY_B)})}
rule(M,BAY_T,W-M,BAY_T);rule(M,BAY_B,W-M,BAY_B);
// Row 2. CV IN sums with the knob: pitch 1 V per semitone (5 V is about a fourth), HAT DECAY and SD SNAPPY 5 V for the full knob.
const CVIN=[["BD1 PITCH","BD1 Pitch CV"],["BD2 PITCH","BD2 Pitch CV"],["SD PITCH","SD Pitch CV"],["TOM PITCH","Tom Pitch CV (the three toms)"],["HAT DECAY","Hat Decay CV (open and closed)"],["SD SNAPPY","SD Snappy CV"]];
{const jy=BAY_B+50,u=84;let x=M;
 T(x+u/2,BAY_B+20,"LFO",11.5,"middle",INK,1);jack("LFO:LFO OUT","LFO OUT",x+u/2,jy,"out","CV","LFO Out");x+=u;rule(x,BAY_B,x,BAY2_B);
 T(x+u*CVIN.length/2,BAY_B+20,"CV IN",11.5,"middle",INK,1);CVIN.forEach(([lb,nm],i)=>jack("CV:"+lb,lb,x+u*(i+.5),jy,"in","CV",nm));x+=u*CVIN.length;rule(x,BAY_B,x,BAY2_B);
 T(x+18,BAY_B+44,"LFO OUT: set it on the LFO tab.  CV IN: sums with the knob; 5 V of pitch is about a fourth, 5 V of decay or snappy is the full knob.",10,"start","#9a9a90",.3)}
S.push(`<rect x="${M}" y="${BAY2_B}" width="${AV}" height="${FR_B-BAY2_B}" fill="url(#cabi)"/>`);rule(M,BAY2_B,W-M,BAY2_B);

// ---- the top bar: SHOGUN, kit, pattern, chain, transport, BAY, track, SOLO. It stays on every tab; BAY is on the RACK tab only.
zone("top");
const TY=30;   // key and screen centre line
T(M+4,TY+7,"SHOGUN",17,"start","#ffffff",4);
const SCR={x:568,y:TY-15,w:220,h:30},TRK={x:1336,y:TY-15,w:84,h:30},KITR={x:288,y:TY-15,w:120,h:30},CHN={x:1044,y:TY-15,w:64,h:30};
T(SCR.x-10,TY+5,"PATTERN",10.5,"end");T(TRK.x-10,TY+5,"TRACK",10.5,"end");T(KITR.x-10,TY+5,"KIT",10.5,"end");T(CHN.x-10,TY+5,"CHAIN",10.5,"end");
[SCR,TRK,KITR,CHN].forEach(r=>lcdBox(r.x,r.y,r.w,r.h));live("SCR");live("TRK");live("KIT");live("CHN");
// a screen is a click target: PATTERN, TRACK and KIT open their lists, CHAIN turns the chain on or off
[["SCR",SCR],["TRK",TRK],["KIT",KITR],["CHN",CHN]].forEach(([id,r])=>ctl({id,kind:"screen",x:r.x+r.w/2,y:r.y+r.h/2,rw:r.w/2,rh:r.h/2,rect:1}));
const btn=(id,cx,g,z=11,gy=TY+4)=>{S.push(`<rect x="${cx-12}" y="${TY-12}" width="24" height="24" rx="3" fill="url(#bs)" stroke="#6f6a5a" stroke-width=".9"/><rect x="${cx-12}" y="${TY+8}" width="24" height="4" rx="2" fill="#7e7764" opacity=".55"/><text x="${cx}" y="${gy}" font-size="${z}" font-weight="700" text-anchor="middle" fill="#2a2620">${g}</text>`);ctl({id,kind:"btn",x:cx,y:TY,r:13,rect:1})};
btn("LEARN",160,"M");ledAt({id:"LED:LEARN"},179,TY,3.5);T(188,TY+5,"LEARN",10.5,"start");
btn("KSAVE",424,"+",17,TY+5);T(440,TY+5,"SAVE",10.5,"start");
btn("PREV",806,"◀");btn("NEXT",832,"▶");btn("SAVE",860,"+",17,TY+5);T(876,TY+5,"SAVE",10.5,"start");btn("COPY",928,"C",12,TY+5);T(944,TY+5,"COPY",10.5,"start");
btn("CH:ADD",1124,"+",17,TY+5);btn("CH:DEL",1150,"−",15,TY+5);
key("START",1190,TY-2,null,{id:"LED:RUN",x:1214,y:TY-14});T(1190,TY+28,"START/STOP",8.5);
zone("baykey");key("BAY",1250,TY-2,null,{id:"LED:BAY",x:1274,y:TY-14});T(1250,TY+28,"BAY",9.5);zone("top");
btn("TPREV",1436,"◀");btn("TNEXT",1462,"▶");
// TEST plays the selected track's voice once
btn("TEST",1492,"▷",12);T(1492,TY+26,"TEST",8.5);
key("SOLO",1534,TY-2,null,{id:"LED:SOLO",x:1558,y:TY-14});T(1534,TY+28,"SOLO",9.5);


// ================= the LFO tab: one tempo-synced LFO for the rack =================
// Hz = BPM / 60 * cycles per beat (1/16 at 120 BPM is 8 Hz). It restarts at phase 0 when the transport starts. Bipolar
// shapes, -1 to 1; AMOUNT scales the jack to 0 to 5 V around 2.5 V, and AMOUNT 0 is 0 V. Its one output is LFO OUT in the
// bay: it reaches nothing without a cable. The settings are kept in this browser, not in the pattern.
zone("lfo");
const LDIV=[["1/1",.25],["1/2",.5],["1/4",1],["1/8",2],["1/8.",4/3],["1/8T",3],["1/16",4],["1/16.",8/3],["1/16T",6],["1/32",8]],LSHAPE=["SINE","TRI","SAW","SQUARE","S+H"];
const lfoDiv=()=>LDIV[Math.round(P["LFO:DIV"]*9)],lfoShapeI=()=>Math.round(P["LFO:SHAPE"]*4),lfoHz=()=>bpmOf(P["CLOCK:TEMPO"])/60*lfoDiv()[1];
const divText=d=>d[0].replace(/\.$/," dotted").replace(/T$/," triplet");
// The LFO's phase as the engine last reported it (about 47 times a second), and when. A fast LFO turns more than a cycle
// between reports, so the scope's playhead runs the phase on from the last report at the LFO's rate. lfoV: LFO OUT in volts.
let lfoV=0,lfoP=0,lfoT=-1,lfoCyc=0,lfoLastP=0;
{const y0=FR_T,cy=y0+170,KX=[110,290,470,650];
 T(M+18,y0+27,"LFO",14,"start",INK,1.5);T(M+66,y0+27,"ONE TEMPO-SYNCED LFO FOR THE RACK · ITS OUTPUT IS THE LFO OUT JACK IN THE BAY",10,"start","#9a9a90",.4);
 rule(M,y0+40,W-M,y0+40);rule(890,y0+40,890,y0+262);
 knob("LFO:DIV","DIVISION",KX[0],cy,6/9,{r:28,n:10,lz:11.5,ly:26,name:"LFO · DIVISION",fmt:()=>divText(lfoDiv())+" · "+lfoHz().toFixed(2)+" Hz at "+bpmOf(P["CLOCK:TEMPO"]).toFixed(1)+" BPM"});lcd("LDIV",KX[0]-50,y0+62,100,null,5);
 knob("LFO:SHAPE","SHAPE",KX[1],cy,0,{r:28,n:5,lz:11.5,ly:26,name:"LFO · SHAPE",fmt:v=>["sine","triangle","saw","square, width 0.5","sample and hold, a new random level each cycle"][Math.round(v*4)]});lcd("LSHAPE",KX[1]-50,y0+62,100,null,6);
 knob("LFO:PHASE","PHASE",KX[2],cy,0,{r:28,lz:11.5,ly:26,name:"LFO · PHASE",fmt:v=>v.toFixed(2)+" of a cycle"});lcd("LPHASE",KX[2]-50,y0+62,100,null,4);
 knob("LFO:AMOUNT","AMOUNT",KX[3],cy,.5,{r:28,lz:11.5,ly:26,name:"LFO · AMOUNT",fmt:v=>v<.0005?"0 V":"0 to "+(5*v).toFixed(2)+" V"+(v>.9995?", around 2.5 V":"")});lcd("LAMT",KX[3]-50,y0+62,100,null,4);
 lcd("LRATE",728,y0+62,140,"RATE",8);
 // the scope: two cycles of the shape as set, from the start of a cycle, and a playhead
 const sx=912,sy=y0+58,sw=W-M-18-sx,sh=176;S.push(`<rect x="${sx}" y="${sy}" width="${sw}" height="${sh}" rx="3" fill="#070807" stroke="#2a2a2c" stroke-width="1.2"/>`);
 [0,2.5,5].forEach(v=>{const y=sy+sh-10-v/5*(sh-20);S.push(`<line x1="${sx+34}" y1="${y}" x2="${sx+sw-28}" y2="${y}" stroke="#1d3a24" stroke-width="1" stroke-dasharray="${v==2.5?"4 4":"none"}"/>`);T(sx+28,y+3.5,v+" V",9,"end","#6f7a66")});
 [0,1,2].forEach(i=>{const x=sx+34+(sw-62)*i/2;S.push(`<line x1="${x}" y1="${sy+10}" x2="${x}" y2="${sy+sh-10}" stroke="#24452c" stroke-width="1"/>`)});
 [0,1].forEach(i=>T(sx+34+(sw-62)*(i+.5)/2,sy+sh+15,"CYCLE "+(i+1),9.5,"middle","#9a9a90",.5));
 live("LFO:SCOPE");LIVE["LFO:SCOPE"]={x:sx+34,y:sy+10,w:sw-62,h:sh-20}}
function lfoShapeAt(i,p,k){if(i==1)return 1-4*Math.abs(p-.5);if(i==2)return 2*p-1;if(i==3)return p<.5?1:-1;if(i==4)return[.55,-.7,.2,-.15,.9][k%5];return Math.sin(2*Math.PI*p)}

// ================= the VOICE tab: pick a device, then a voice; large knobs for that voice only =================
// Every knob here is the same control as on the face (or a kit-only control the face does not show: BD 1 and CLAP
// TRANSIENT, the WAVE folders). The face on the RACK tab is unchanged.
zone("voice");
const VPX=330,VPW=(W-M-14-VPX)/16;
{const y0=FR_T;T(M+18,y0+27,"VOICE",14,"start",INK,1.5);T(M+86,y0+27,"PICK A DEVICE, THEN A VOICE · THESE ARE THE SAME CONTROLS AS THE FACE",10,"start","#9a9a90",.4);
 rule(M,y0+40,W-M,y0+40);T(M+18,y0+68,"DEVICE",10.5,"start");lcdBox(M+76,y0+50,130,30);live("VDEV");
 T(VPX-12,y0+68,"VOICE",10.5,"end");live("VSEL");
 VOICES.forEach((v,i)=>ctl({id:"VSEL:"+v.k,kind:"sel",voice:v.k,x:VPX+VPW*(i+.5),y:y0+65,rw:VPW/2-2,rh:15,rect:1}));
 rule(M,y0+96,W-M,y0+96)}
const VEXTRA={BD1:[["SOUND","TRANSIENT",x=>"transient "+(1+Math.round(x*15))+" of 16"]],CP:[["SOUND","TRANSIENT",x=>"transient "+(1+Math.round(x*15))+" of 16"]]};
const VCV={BD1:"CV IN · BD1 PITCH adds to TUNE: 5 V is about a fourth up",BD2:"CV IN · BD2 PITCH adds to TUNE: 5 V is about a fourth up",
  SD:"CV IN · SD PITCH adds to TUNE (5 V is about a fourth up) · SD SNAPPY adds to SNAPPY (5 V is the whole knob)",
  LTC:"CV IN · TOM PITCH adds to TUNE on all three toms: 5 V is about a fourth up",OH:"CV IN · HAT DECAY adds to DECAY on both hats (5 V is the whole knob, on the same short decay curve)"};
VCV.MTC=VCV.HTC=VCV.LTC;VCV.HH=VCV.OH;
AL=true;
VOICES.forEach(v=>{zone("v_"+v.k);const y0=FR_T,cy=y0+164,items=[];
  v.knobs.forEach(([lb,p,n,idn])=>{const id=v.k+":"+(idn||lb);if(!p)return;items.push(n=="tog"?{tog:id,lb,marks:["OFF","ON"],name:v.t+" · NOISE"}:{id,lb})});
  (VEXTRA[v.k]||[]).forEach(([h,lb,fmt])=>items.push({id:v.k+":"+h,lb,fmt,name:v.t+" · "+lb}));
  if(v.tog){items.push({tog:v.k+":MODE",lb:"TOM / CGA",marks:["TOM","CGA"],name:v.t+" · TOM or CONGA"});items.push({id:"TOMS:NOISE",lb:"NOISE LVL"})}
  items.push({id:v.k+":LEVEL",lb:"LEVEL"});
  const sp=Math.min(150,(AV-40)/items.length),x0=W/2-sp*(items.length-1)/2;
  items.forEach((it,i)=>{const cx=x0+sp*i;
    if(it.tog){toggle(it.tog,cx,cy,it.marks,0,it.name);T(cx,cy+44,it.lb,11.5);return}
    const fc=CTRL.find(c=>c.id==it.id&&c.kind=="knob");
    knob(it.id,it.lb,cx,cy,0,{r:30,lz:11.5,ly:26,n:STEPS[it.id],name:it.name||(fc&&fc.name),fmt:it.fmt||(fc&&fc.fmt)})});
  T(M+18,y0+252,VCV[v.k]||"No CV input on this voice",10.5,"start","#9a9a90",.3)});
AL=false;

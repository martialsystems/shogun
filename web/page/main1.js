// ================= geometry =================
const NS="http://www.w3.org/2000/svg",W=1600,M=14,AV=W-2*M,FR_T=54,RH=[244,244,196,176],INK="#f4f3ee",GRN="#4aa862",JR=9;
const ROWY=RH.reduce((a,h,i)=>(a.push(i?a[i-1]+RH[i-1]:FR_T),a),[]),FR_B=ROWY[3]+RH[3],H=FR_B+16,LANE=110,VH=H+LANE;
const $=id=>document.getElementById(id),sv=$("sv"),cab=$("cab"),ring=$("ring"),info=$("info"),menu=$("menu"),stage=$("stage");
sv.setAttribute("viewBox",`0 0 ${W} ${VH}`);
const clamp=(v,a=0,b=1)=>Math.max(a,Math.min(b,v));
// Panel defaults: the example values printed in SCHEMATICS.md, by engine knob name (shogun::Knobs).
const KD={bd1Attack:64,bd1Decay:80,bd1Pitch:40,bd1Tune:50,bd1Noise:0,bd1Filter:64,bd1Dist:0,bd1Trigger:0,bd2Tune:60,bd2Decay:70,bd2Tone:100,
  sdTune:70,sdDTune:90,sdPitch:30,sdTone:64,sdToneDecay:60,sdSnappy:70,sdSnDecay:50,rsTune:40,cpAttack:100,cpDecay:50,cpFilter:64,cpTrigger:0,cpData:48,
  clTune:50,clDecay:30,cyTune:64,cyTone:70,cyDecay:90,ohDecay:100,hhTune:60,hhDecay:40,ltcTune:40,ltcDecay:60,mtcTune:50,mtcDecay:60,htcTune:60,htcDecay:60,
  tomNoise:33,cbTune:48,cbDecay:70,maDecay:55,leadTone:80,bassTone:64};
const VI=["BD1","BD2","SD","RS","CY","OH","HH","CL","CP","LTC","MTC","HTC","CB","MA","LEAD","BASS"];   // shogun::Voice order

// ================= the instrument: the jobs of the design pack, on a new plate =================
// knobs: [label, engine param, positions] (positions = a stepped control; "tog" = an OFF/ON switch). LEVEL is the linear level.
const VOICES=[
 {k:"BD1",t:"BD 1",row:0,cols:5,knobs:[["ATTACK","bd1Attack"],["DECAY","bd1Decay"],["PITCH","bd1Pitch"],["TUNE","bd1Tune"],["NOISE","bd1Noise"],["FILTER","bd1Filter"],["DIST","bd1Dist"],["SOUND","bd1Trigger",16],["LEVEL"]]},
 {k:"BD2",t:"BD 2",row:0,cols:2,knobs:[["TUNE","bd2Tune"],["DECAY","bd2Decay"],["TONE","bd2Tone"],["LEVEL"]]},
 {k:"SD",t:"SNARE",row:0,cols:4,knobs:[["TUNE","sdTune"],["D-TUNE","sdDTune"],["PITCH","sdPitch"],["TONE","sdTone"],["T.DECAY","sdToneDecay"],["SNAPPY","sdSnappy"],["SN.DECAY","sdSnDecay"],["LEVEL"]]},
 {k:"RS",t:"RIM",row:0,cols:1,knobs:[["TUNE","rsTune"],["LEVEL"]]},
 {k:"CP",t:"CLAP",row:0,cols:3,knobs:[["ATTACK","cpAttack"],["DECAY","cpDecay"],["FILTER","cpFilter"],["SOUND","cpTrigger",16],["COUNT","cpData",8],["LEVEL"]]},
 {k:"CL",t:"CLAVES",row:0,cols:2,knobs:[["TUNE","clTune"],["DECAY","clDecay"],["LEVEL"]]},
 {k:"CY",t:"CYMBAL",row:1,cols:2,knobs:[["TUNE","cyTune"],["TONE","cyTone"],["DECAY","cyDecay"],["LEVEL"]]},
 {k:"OH",t:"OP HAT",row:1,cols:1,knobs:[["DECAY","ohDecay"],["LEVEL"]]},
 {k:"HH",t:"CL HAT",row:1,cols:2,knobs:[["TUNE","hhTune"],["DECAY","hhDecay"],["LEVEL"]]},
 {k:"LTC",t:"LOW TOM",row:1,cols:2,tog:1,knobs:[["TUNE","ltcTune"],["DECAY","ltcDecay"],["NOISE","ltcNoise","tog"],["LEVEL"]]},
 {k:"MTC",t:"MID TOM",row:1,cols:2,tog:1,knobs:[["TUNE","mtcTune"],["DECAY","mtcDecay"],["NOISE","mtcNoise","tog"],["LEVEL"]]},
 {k:"HTC",t:"HI TOM",row:1,cols:2,tog:1,knobs:[["TUNE","htcTune"],["DECAY","htcDecay"],["NOISE","htcNoise","tog"],["LEVEL"]]},
 {k:"CB",t:"COWBELL",row:1,cols:2,knobs:[["TUNE","cbTune"],["DECAY","cbDecay"],["LEVEL"]]},
 {k:"MA",t:"MARACAS",row:1,cols:1,knobs:[["DECAY","maDecay"],["LEVEL"]]},
 {k:"LEAD",t:"LEAD",row:2,cols:2,note:1,knobs:[["TONE","leadTone"],["LEVEL"]]},
 {k:"BASS",t:"BASS",row:2,cols:2,note:1,knobs:[["TONE","bassTone"],["LEVEL"]]}];
const VK=Object.fromEntries(VOICES.map(v=>[v.k,v])),NOTEK={LEAD:1,BASS:1},BENDK={BD1:1,BD2:1,SD:1,LTC:1,MTC:1,HTC:1};
const P={},DEF={},CTRL=[],JACKS={},LIVE={},MAP={};   // panel values 0..1, defaults, hit regions, jacks, live svg groups, panel id -> engine target
const S=[];
const T=(x,y,s,z=11,anchor="middle",fill=INK,ls=.5)=>S.push(`<text x="${x.toFixed(1)}" y="${y.toFixed(1)}" font-size="${z}" font-weight="700" letter-spacing="${ls}" text-anchor="${anchor}" fill="${fill}">${s}</text>`);
const live=(id,x=0,y=0)=>{LIVE[id]={x,y};S.push(`<g id="L_${id.replace(/[^A-Za-z0-9]/g,"_")}"></g>`)};
const lv=id=>document.getElementById("L_"+id.replace(/[^A-Za-z0-9]/g,"_"));
function ticks(cx,cy,r,n=11){for(let i=0;i<n;i++){const a=(-135+270*i/(n-1)-90)*Math.PI/180,l=i==0||i==n-1||i==(n-1)/2?4:2.5;
  S.push(`<line x1="${(cx+(r+4)*Math.cos(a)).toFixed(1)}" y1="${(cy+(r+4)*Math.sin(a)).toFixed(1)}" x2="${(cx+(r+4+l)*Math.cos(a)).toFixed(1)}" y2="${(cy+(r+4+l)*Math.sin(a)).toFixed(1)}" stroke="${INK}" stroke-width="1.1"/>`)}}
const STEPS={};   // stepped controls: id -> positions
function knob(id,label,cx,cy,def,opt={}){const r=opt.r||14;P[id]=DEF[id]=def;if(opt.n)STEPS[id]=opt.n;ticks(cx,cy,r,opt.n&&opt.n<=16?opt.n:11);live(id);LIVE[id]={x:cx,y:cy,r};
  if(opt.marks)opt.marks.forEach((m,i)=>{const a=(-135+270*i/(opt.marks.length-1)-90)*Math.PI/180;T(cx+(r+16)*Math.cos(a),cy+(r+16)*Math.sin(a)+3,m,8.5)});
  if(label)T(cx,cy+r+19,label,10);CTRL.push({id,kind:"knob",x:cx,y:cy,r:r+8,name:opt.name||id,fmt:opt.fmt})}
function key(id,cx,cy,label,lamp){S.push(`<rect x="${cx-17}" y="${cy-17}" width="34" height="34" rx="4" fill="#050505" stroke="#262826" stroke-width="1.2"/>`);live(id);LIVE[id]={x:cx,y:cy};
  if(label)T(cx,cy+30,label,10);if(lamp)ledAt(lamp,lamp.x,lamp.y);const c={id,kind:"key",x:cx,y:cy,r:17,rect:1};CTRL.push(c);return c}
function ledAt(l,cx,cy,r=4.5){S.push(`<circle cx="${cx}" cy="${cy}" r="${r+2}" fill="#050505" stroke="#262826"/>`);live(l.id);LIVE[l.id]={x:cx,y:cy,r}}
function jack(id,label,cx,cy,dir,type,name){S.push(`<circle cx="${cx}" cy="${cy}" r="${JR+1.5}" fill="#000" opacity=".55"/><circle cx="${cx}" cy="${cy}" r="${JR}" fill="url(#js)" stroke="#2a2a2c" stroke-width=".9"/><circle cx="${cx}" cy="${cy}" r="${JR-2.6}" fill="url(#jn)"/><circle cx="${cx}" cy="${cy}" r="${JR-5}" fill="#030303"/>`);
  T(cx,cy+24.5,label,9.5);JACKS["SHOGUN/"+id]={x:cx,y:cy,dir,type,name}}
function toggle(id,cx,cy,marks,def,name){P[id]=DEF[id]=def;S.push(`<rect x="${cx-15}" y="${cy-8}" width="30" height="16" rx="3" fill="#050505" stroke="#262826"/><rect x="${cx-10}" y="${cy-2}" width="20" height="4" rx="2" fill="#000"/><circle cx="${cx}" cy="${cy}" r="5" fill="url(#jn)" stroke="#111" stroke-width=".8"/>`);
  T(cx-22,cy+4,marks[0],9,"end");T(cx+22,cy+4,marks[1],9,"start");live(id);LIVE[id]={x:cx,y:cy};CTRL.push({id,kind:"toggle",x:cx,y:cy,r:18,marks,name:name||id})}
const lcdBox=(x,y,w,h)=>S.push(`<rect x="${x}" y="${y}" width="${w}" height="${h}" rx="3" fill="#0a0a0b" stroke="#2a2a2c" stroke-width="1.2"/><rect x="${x+4}" y="${y+4}" width="${w-8}" height="${h-8}" rx="1.5" fill="url(#lcd)"/>`);
const LCD={};function lcd(id,x,y,w,label,chars){lcdBox(x,y,w,28);LCD[id]={x:x+6,y:y+6,w:w-12,h:16,n:chars};live("LCD:"+id);if(label)T(x+w/2,y+43,label,10)}

// ---- sections: rows 0 to 2 share their width by minimum size
const minW=s=>s.min||Math.max(s.cols*62,120);
const ROWS=[VOICES.filter(v=>v.row==0),VOICES.filter(v=>v.row==1).concat([]),VOICES.filter(v=>v.row==2).concat([{k:"OUT",min:640},{k:"CLOCK",min:470}])];
ROWS[1].splice(ROWS[1].findIndex(s=>s.k=="CB"),0,{k:"TOMS",min:110});
const rules=[],hlines=ROWY.slice(1);
ROWS.forEach((row,ri)=>{const sc=AV/row.reduce((a,s)=>a+minW(s),0);let x=M;row.forEach((s,i)=>{s.x=x;s.w=minW(s)*sc;s.y=ROWY[ri];x+=s.w;if(i<row.length-1)rules.push([x,ROWY[ri],ROWY[ri]+RH[ri]])})});
const ccOf=(id,v)=>{const n=STEPS[id];if(!n)return Math.round(v*127);const i=Math.round(v*(n-1));return Math.floor(i*128/n)};   // a stepped control sends the lowest controller value of its position
const vOf=(id,cc,n)=>n?Math.floor(cc*n/128)/(n-1):cc/127;
function voiceSection(v){const{x,w,y}=v,cx=x+w/2,cw=w/v.cols,note=v.note;T(cx,y+22,v.t,12.5,"middle",INK,1);
  const kx=v.tog?cx-58:cx-10;const c=key("SEL:"+v.k,kx,y+52,null,{id:"LED:"+v.k,x:kx+26,y:y+52});c.voice=v.k;
  if(v.tog){toggle(v.k+":MODE",cx+44,y+52,["TOM","CGA"],0,v.t+" · TOM or CONGA");MAP[v.k+":MODE"]={f:v.k.toLowerCase()+"Mode",tog:1}}
  v.knobs.forEach(([lb,p,n],i)=>{const c=i%v.cols,r=Math.floor(i/v.cols),kx=x+cw*(c+.5),ky=y+(note?104:96)+r*58,id=v.k+":"+lb;
    if(n=="tog"){toggle(id,kx,ky,["OFF","ON"],0,v.t+" · NOISE");T(kx,ky+33,lb,10);MAP[id]={f:p,tog:1};return}
    if(!p){knob(id,lb,kx,ky,1,{name:v.t+" · LEVEL"});MAP[id]={level:VI.indexOf(v.k)};return}
    knob(id,lb,kx,ky,vOf(id,KD[p],n),{n,name:v.t+" · "+lb+(p=="hhTune"?" (open and closed hat colour)":""),fmt:n==16?x=>"transient "+(1+Math.round(x*15))+" of 16":n==8?x=>(1+Math.round(x*7))+" bursts":null});MAP[id]={f:p}});
  if(!note)jack(v.k+":TRIG","TRIG",cx,y+206,"in","Gate",v.t+" trig in")}
VOICES.forEach(voiceSection);
const sec=k=>ROWS.flat().find(s=>s.k==k);
{const s=sec("TOMS"),cx=s.x+s.w/2;T(cx,s.y+22,"TOMS",12.5,"middle",INK,1);knob("TOMS:NOISE","NOISE",cx,s.y+96,KD.tomNoise/127,{name:"TOMS · NOISE level, shared by the three"});MAP["TOMS:NOISE"]={f:"tomNoise"};
 T(cx,s.y+150,"ALL THREE",9,"middle","#bdbcb4")}
{const s=sec("OUT"),y=s.y,x=s.x,sp=(s.w-120)/7;T(x+s.w/2,y+22,"OUTPUTS",12.5,"middle",INK,1);knob("OUT:MASTER","MASTER",x+58,y+100,.65,{name:"MASTER (scales MAIN only)"});MAP["OUT:MASTER"]={master:1};
 [["BD","BD"],["SD/RS","SDRS"],["HH/CY","HHCY"],["CP","CP"],["TO/CO","TOCO"],["CB/CL","CBCL"],["MAIN","MAIN"]].forEach(([lb,k],i)=>{const cx=x+120+sp*(i+.5);T(cx,y+72,lb,11,"middle",INK,.8);
   S.push(`<line x1="${cx-22}" y1="${y+80}" x2="${cx+22}" y2="${y+80}" stroke="${INK}" stroke-width="1"/>`);["L","R"].forEach((c,j)=>jack("OUT:"+k+" "+c,c,cx+(j?17:-17),y+108,"out","Audio",lb+" out "+(j?"right":"left")))})}
const bpmOf=v=>60+v*120,SCALEN=["32ND","16T","16TH","8T"];
{const s=sec("CLOCK"),y=s.y,x=s.x,w=s.w;T(x+w/2,y+22,"CLOCK",12.5,"middle",INK,1);
 knob("CLOCK:TEMPO","TEMPO",x+44,y+76,.5,{name:"TEMPO",fmt:v=>bpmOf(v).toFixed(1)+" BPM"});lcd("BPM",x+76,y+62,66,"BPM",3);CTRL.push({id:"CLOCK:TEMPO",kind:"knob",x:x+109,y:y+76,r:30,name:"TEMPO",fmt:v=>bpmOf(v).toFixed(1)+" BPM"});
 toggle("CLOCK:SOURCE",x+w*.42,y+76,["INT","EXT"],0,"CLOCK");T(x+w*.42,y+109,"CLOCK",10);
 knob("CLOCK:SCALE","SCALE",x+w*.6,y+76,2/3,{n:4,marks:["32","16T","16","8T"],name:"SCALE",fmt:v=>["32nd notes, 8 steps per beat","16th triplets, 6 steps per beat","16th notes, 4 steps per beat","8th triplets, 3 steps per beat"][Math.round(v*3)]});
 knob("CLOCK:BAR","BAR",x+w*.74,y+76,15/31,{n:32,name:"BAR",fmt:v=>(1+Math.round(v*31))+" steps per bar"});lcd("POS",x+w*.82,y+62,56,"STEP",2);
 [["CLK IN","in","clock in"],["RST IN","in","reset in"],["RUN IN","in","start/stop in"],["CLK OUT","out","clock out"]].forEach(([lb,d,nm],i)=>jack("CLOCK:"+lb,lb,x+w*(.14+i*.24),y+150,d,"Gate","CLOCK "+nm))}
// row 3: TRANSPORT, TRACK, STEP and the 16 step keys
const TRW=118,TKW=330,STW=300,SX=M+TRW+TKW+STW,SW=(AV-TRW-TKW-STW)/16,y3=ROWY[3];rules.push([M+TRW,y3,FR_B],[M+TRW+TKW,y3,FR_B],[SX,y3,FR_B]);
T(M+TRW/2,y3+22,"TRANSPORT",12.5,"middle",INK,1);key("START",M+TRW/2,y3+68,"START/STOP",{id:"LED:RUN",x:M+TRW/2+30,y:y3+56});key("CLEAR",M+TRW/2-27,y3+128,"CLEAR");key("UNDO",M+TRW/2+27,y3+128,"UNDO");
{const x=M+TRW;T(x+TKW/2,y3+22,"TRACK",12.5,"middle",INK,1);
 knob("SEQ:LENGTH","LENGTH",x+38,y3+66,15/31,{n:32,name:"LENGTH",fmt:v=>(1+Math.round(v*31))+" steps"});lcd("LEN",x+66,y3+52,62,"STEPS",2);CTRL.push({id:"SEQ:LENGTH",kind:"knob",x:x+97,y:y3+66,r:30,name:"LENGTH",fmt:v=>(1+Math.round(v*31))+" steps"});
 knob("SEQ:SHUFFLE","SHUFFLE",x+170,y3+66,0,{n:16,name:"SHUFFLE",fmt:v=>"intensity "+Math.round(v*15)+" of 15"});
 knob("SEQ:SHIFT","SHIFT",x+232,y3+66,0,{name:"SHIFT",fmt:v=>(Math.round(v*127)/127*30).toFixed(1)+" ms later"});
 key("MUTE",x+288,y3+66,"MUTE",{id:"LED:MUTE",x:x+312,y:y3+48});
 key("PAGE:0",x+100,y3+132,"1-16",{id:"LED:P0",x:x+124,y:y3+120});key("PAGE:1",x+180,y3+132,"17-32",{id:"LED:P1",x:x+204,y:y3+120});key("RANDOM",x+270,y3+132,"RANDOM");key("SOLO",x+34,y3+132,"SOLO",{id:"LED:SOLO",x:x+58,y:y3+120})}
{const x=M+TRW+TKW;T(x+STW/2,y3+22,"STEP",12.5,"middle",INK,1);lcd("EDIT",x+16,y3+52,62,"EDIT",2);
 knob("STEP:FLAM","FLAM",x+124,y3+66,0,{n:17,name:"FLAM",fmt:v=>{const i=Math.round(v*16);return i?"flam "+i+" of 16":"no flam"}});
 knob("STEP:BEND","BEND",x+186,y3+66,.5,{name:"BEND",fmt:v=>Math.abs(v-.5)<.006?"no bend":((12*(2*Math.round(v*127)/127-1))>=0?"+":"")+(12*(2*Math.round(v*127)/127-1)).toFixed(2)+" semitones"});
 knob("STEP:NOTE","NOTE",x+248,y3+66,24/36,{n:37,name:"NOTE",fmt:v=>noteName(36+Math.round(v*36))});
 key("TIE",x+124,y3+132,"TIE",{id:"LED:TIE",x:x+148,y:y3+120});T(SX-8,y3+133,"ACC",9,"end");T(SX-8,y3+156,"LOCK",9,"end")}
for(let i=0;i<16;i++){const cx=SX+SW*(i+.5);live("NUM:"+i);LIVE["NUM:"+i]={x:cx,y:y3+22};
  S.push(`<circle cx="${cx}" cy="${y3+46}" r="9" fill="url(#js)" stroke="#2a2a2c" stroke-width=".9"/><circle cx="${cx}" cy="${y3+46}" r="6.5" fill="#050505"/>`);live("PH:"+i);LIVE["PH:"+i]={x:cx,y:y3+46};
  const c=key("STEP:"+i,cx,y3+88);c.step=i;ledAt({id:"ACC:"+i},cx,y3+130,4);CTRL.push({id:"ACC:"+i,kind:"acc",x:cx,y:y3+130,r:10,step:i});live("LK:"+i);LIVE["LK:"+i]={x:cx,y:y3+156};
  if(i%4==0&&i)rules.push([SX+SW*i,y3,FR_B])}
const NN=["C","C#","D","D#","E","F","F#","G","G#","A","A#","B"],noteName=n=>NN[n%12]+(Math.floor(n/12)-1);
// top band: name, pattern screen, track screen
T(M+4,36,"SHOGUN",17,"start","#ffffff",4);
const SCR={x:604,y:13,w:262,h:30},TRK={x:1226,y:13,w:100,h:30},KITR={x:314,y:13,w:150,h:30},CHN={x:1452,y:13,w:76,h:30};
T(SCR.x-12,33,"PATTERN",10.5,"end");T(TRK.x-12,33,"TRACK",10.5,"end");T(KITR.x-12,33,"KIT",10.5,"end");T(CHN.x-12,33,"CHAIN",10.5,"end");
[SCR,TRK,KITR,CHN].forEach(r=>lcdBox(r.x,r.y,r.w,r.h));live("SCR");live("TRK");live("KIT");live("CHN");
// a screen is a click target: PATTERN, TRACK and KIT open their lists, CHAIN turns the chain on or off
[["SCR",SCR],["TRK",TRK],["KIT",KITR],["CHN",CHN]].forEach(([id,r])=>CTRL.push({id,kind:"screen",x:r.x+r.w/2,y:r.y+r.h/2,rw:r.w/2,rh:r.h/2,rect:1}));
const btn=(id,cx,g,z=11,gy=32)=>{S.push(`<rect x="${cx-12}" y="16" width="24" height="24" rx="3" fill="url(#bs)" stroke="#6f6a5a" stroke-width=".9"/><rect x="${cx-12}" y="36" width="24" height="4" rx="2" fill="#7e7764" opacity=".55"/><text x="${cx}" y="${gy}" font-size="${z}" font-weight="700" text-anchor="middle" fill="#2a2620">${g}</text>`);CTRL.push({id,kind:"btn",x:cx,y:28,r:13,rect:1})};
btn("LEARN",164,"M",11,32);ledAt({id:"LED:LEARN"},183,28,3.5);T(193,33,"LEARN",10.5,"start");
btn("KSAVE",484,"+",17,33);T(500,33,"SAVE",10.5,"start");
btn("PREV",884,"◀");btn("NEXT",912,"▶");btn("SAVE",944,"+",17,33);T(960,33,"SAVE",10.5,"start");btn("COPY",1010,"C",12,33);T(1026,33,"COPY",10.5,"start");
btn("TPREV",1346,"◀");btn("TNEXT",1374,"▶");btn("CH:ADD",1546,"+",17,33);btn("CH:DEL",1572,"−",15,33);

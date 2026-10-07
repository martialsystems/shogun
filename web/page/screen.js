// ================= PATTERN and TRACK screens: dropdown lists like RONIN's PRESET screen =================
// Click a screen for its list, type to search, arrows or scroll, then click a row or press Enter. Esc closes.
// Patterns: banks A and B of up to 999. Bank A starts with the factory patterns; the lit bank lamp is the bank you browse,
// step through with the arrows, and SAVE into (type a name, then Enter or SAVE again). Saved patterns stay in this browser.
const DD={open:null,query:"",name:"",hi:0,top:0},DDROWS=8,DDROW=20,DDPAD=5,MAXP=999;
const ddRect=()=>DD.open=="PAT"?{x:SCR.x,w:SCR.w,y:SCR.y+SCR.h+4}:{x:TRK.x+TRK.w-190,w:190,y:TRK.y+TRK.h+4};
function ddItems(){const q=DD.query.trim();
  if(DD.open=="PAT")return patList(bankView).map((p,i)=>({i,t:pad3(i+1)+" "+p.n.toUpperCase(),on:curPat.b==bankView&&curPat.i==i})).filter(r=>!q||r.t.includes(q));
  return VOICES.map((v,i)=>({i,t:(v.t+"        ").slice(0,8)+String(tracks[v.k].len).padStart(2)+" STEPS",on:v.k==sel})).filter(r=>!q||r.t.includes(q))}
function ddBox(){const r=ddRect(),n=Math.max(1,Math.min(DDROWS,ddItems().length));return {x:r.x,y:r.y,w:r.w,h:DDPAD*2+n*DDROW-3}}
function ddDraw(){const g=$("dd");if(!DD.open){g.innerHTML="";return}const b=ddBox(),M=ddItems(),n=DD.open=="PAT"?20:17;
  let s=`<rect x="${b.x}" y="${b.y}" width="${b.w}" height="${b.h}" rx="4" fill="#0a0a0b" stroke="${GRN}" stroke-width="1.2"/>`;
  const row=(k,txt,hi)=>{const x=b.x+DDPAD,y=b.y+DDPAD+k*DDROW,w=b.w-DDPAD*2,h=DDROW-3;
    return `<rect x="${x}" y="${y}" width="${w}" height="${h}" rx="1.5" fill="${hi?"#1d2414":"url(#lcd)"}"/>`+dots(x+3,y+1,w-6,h-2,txt,n,hi?"#a6b192":"#1d2414")};
  if(!M.length)s+=row(0,DD.open=="TRK"||patList(bankView).length?" NO MATCH":" BANK "+bankView+" IS EMPTY",false);
  M.slice(DD.top,DD.top+DDROWS).forEach((r,k)=>{s+=row(k,(r.on?">":" ")+r.t,DD.top+k==DD.hi)});
  if(M.length>DDROWS){const h=b.h-8,y=b.y+4+h*DD.top/M.length;s+=`<rect x="${b.x+b.w-4}" y="${y.toFixed(1)}" width="2" height="${(h*DDROWS/M.length).toFixed(1)}" rx="1" fill="${GRN}" opacity=".8"/>`}
  g.innerHTML=s}
function ddKeep(){if(DD.hi<DD.top)DD.top=DD.hi;if(DD.hi>=DD.top+DDROWS)DD.top=DD.hi-DDROWS+1}
function ddOpen(kind){DD.open=kind;DD.query="";const M=ddItems();DD.hi=Math.max(0,M.findIndex(r=>r.on));DD.top=0;ddKeep();ddDraw();drawAll();
  info.textContent=(kind=="PAT"?"Patterns in bank "+bankView:"Tracks")+" · type to search, arrows or scroll, Enter or click · Esc closes"}
function ddClose(){DD.open=null;ddDraw();drawAll()}
function ddChoose(r){const k=DD.open;ddClose();if(!r)return;if(k=="PAT")loadPat(bankView,r.i);else selTrack(VOICES[r.i].k)}
function ddRowAt(x,y){const b=ddBox();if(x<b.x||x>b.x+b.w||y<b.y||y>b.y+b.h)return null;const k=Math.floor((y-b.y-DDPAD)/DDROW);const M=ddItems();return k>=0&&k<DDROWS&&DD.top+k<M.length?M[DD.top+k]:undefined}
function ddScroll(d){const n=ddItems().length;DD.top=Math.max(0,Math.min(Math.max(0,n-DDROWS),DD.top+d));ddDraw()}
const typedCh=e=>e.key.length==1&&!e.ctrlKey&&!e.metaKey&&!e.altKey&&FONT[e.key.toUpperCase()]?e.key.toUpperCase():null;
// true when the key belonged to an open list or to the name being typed
function ddKey(e){const ch=typedCh(e);
  if(DD.naming){if(e.key=="Escape")nameCancel();else if(e.key=="Enter")nameCommit();else if(e.key=="Backspace")DD.name=DD.name.slice(0,-1);else if(ch&&DD.name.length<12&&!(ch==" "&&!DD.name))DD.name+=ch;e.preventDefault();drawAll();return true}
  if(!DD.open)return false;const M=ddItems();
  if(e.key=="Escape")ddClose();
  else if(e.key=="ArrowDown"){if(M.length){DD.hi=(DD.hi+1)%M.length;ddKeep();ddDraw()}}
  else if(e.key=="ArrowUp"){if(M.length){DD.hi=(DD.hi+M.length-1)%M.length;ddKeep();ddDraw()}}
  else if(e.key=="Enter")ddChoose(M[DD.hi]);
  else if(e.key=="Backspace"){DD.query=DD.query.slice(0,-1);DD.hi=0;DD.top=0;ddDraw();drawAll()}
  else if(ch&&DD.query.length<12){DD.query+=ch;DD.hi=0;DD.top=0;ddDraw();drawAll()}
  else return true;
  e.preventDefault();return true}
function nameStart(){if(patList(bankView).length>=MAXP){info.textContent=`Bank ${bankView} is full (${MAXP})`;return}ddClose();DD.naming=true;DD.name="";drawAll();info.textContent="Type a name, then Enter (or SAVE again) to save in bank "+bankView+" · Esc cancels"}
function nameCancel(){DD.naming=false;drawAll()}
function nameCommit(){DD.naming=false;savePat((DD.name.trim()||"PATTERN").slice(0,12));drawAll()}
// what the PATTERN screen shows: the loaded pattern, the search being typed, or the name being typed
function scrText(){if(DD.naming)return bankView+pad3(patList(bankView).length+1)+" "+DD.name+"_";if(DD.open=="PAT")return "FIND "+DD.query+"_";
  const p=patList(curPat.b)[curPat.i];return p?curPat.b+pad3(curPat.i+1)+" "+p.n.toUpperCase()+(dirty?"*":""):""}
const trkText=()=>DD.open=="TRK"?("FIND "+DD.query+"_").slice(-8):VK[sel].t;
function selTrack(k){sel=k;if(edit>=tracks[sel].len)edit=0;loadKnobs();drawAll();info.textContent=describe(CTRL.find(c=>c.voice==k))}
function stepTrack(d){const i=VOICES.findIndex(v=>v.k==sel);selTrack(VOICES[(i+d+VOICES.length)%VOICES.length].k)}

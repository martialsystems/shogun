// ================= cables (the Jidai patch-bay model: Verlet rope, stacks on a jack, newest on top) =================
const COL={red:["#d23a30","#6e100c","#ff9a8a"],white:["#ece8da","#86836f","#fff"],yellow:["#eabd2c","#80600a","#fff2a8"],green:["#33a352","#10521f","#98e6aa"]};
$("df").innerHTML+=Object.entries(COL).map(([n,[b,k,l]])=>`<radialGradient id="g_${n}" cx=".38" cy=".32" r=".85"><stop offset="0" stop-color="${l}"/><stop offset=".35" stop-color="${b}"/><stop offset="1" stop-color="${k}"/></radialGradient>`).join("");
let cur="green",cables=[],hov=null,mx=0,my=0,grab=null,down=null,ignoreUp=false,ptrIn=false;const N=36,RR=40,RP=28;
$("sws").innerHTML=Object.keys(COL).map(c=>`<button class="sw${c==cur?" on":""}" data-c="${c}" aria-label="${c} cable" style="background:${COL[c][0]}"></button>`).join("");
$("sws").onclick=e=>{const c=e.target.dataset.c;if(!c)return;cur=c;[...$("sws").children].forEach(b=>b.classList.toggle("on",b.dataset.c==c))};
function plug(x,y,c,k){x-=2*k;y-=3*k;const r=12*(1+.04*k);return `<circle cx="${x+2}" cy="${y+3}" r="${r+1.5}" fill="#000" opacity=".4"/><circle cx="${x}" cy="${y}" r="${r}" fill="url(#g_${c})" stroke="${COL[c][1]}" stroke-width="1.4"/><circle cx="${x}" cy="${y}" r="${r*.62}" fill="url(#sl)" stroke="#3a3a3a" stroke-width=".8"/><circle cx="${x}" cy="${y}" r="${r*.36}" fill="#050505"/><ellipse cx="${x-r*.4}" cy="${y-r*.55}" rx="${r*.38}" ry="${r*.2}" fill="#fff" opacity=".35" transform="rotate(-30 ${x-r*.4} ${y-r*.55})"/>`}
function dstr(p){let s=`M${p[0][0]},${p[0][1]}`;for(let i=1;i<p.length-1;i++)s+=`Q${p[i][0]},${p[i][1]} ${(p[i][0]+p[i+1][0])/2},${(p[i][1]+p[i+1][1])/2}`;return s+`L${p[p.length-1][0]},${p[p.length-1][1]}`}
function pins(c){const Q=[];["a","b"].forEach((e,n)=>{const j=c[e],k=c.ks[n];let x=mx,y=my;if(j){x=JACKS[j].x;y=JACKS[j].y}c.pl[n].setAttribute("transform",`translate(${x} ${y})`);Q.push([x-2*k,y-3*k+4])});c.pa=Q[0];c.pb=Q[1];const d=Math.hypot(Q[1][0]-Q[0][0],Q[1][1]-Q[0][1]);c.L=d+Math.min(d*.3+40,140)}
function build(){cab.innerHTML="";const cnt={};let fresh=false;
  for(const c of cables){c.ks=[0,0];["a","b"].forEach((e,n)=>{if(c[e]){c.ks[n]=cnt[c[e]]||0;cnt[c[e]]=c.ks[n]+1}});
    const g=document.createElementNS(NS,"g"),[b,k,l]=COL[c.c];
    g.innerHTML=`<path fill="none" stroke="${k}" stroke-width="7" stroke-linecap="round"/><path fill="none" stroke="${b}" stroke-width="5.2" stroke-linecap="round"/><path fill="none" stroke="${l}" stroke-width="1.6" opacity=".55" transform="translate(-.8 -1.2)"/><g></g><g></g>`;
    c.el=[g.children[0],g.children[1],g.children[2]];c.pl=[g.children[3],g.children[4]];c.pl[0].innerHTML=plug(0,0,c.c,c.ks[0]);c.pl[1].innerHTML=plug(0,0,c.c,c.ks[1]);pins(c);
    if(!c.p){fresh=true;c.p=[];for(let i=0;i<N;i++){const t=i/(N-1);c.p.push([c.pa[0]+(c.pb[0]-c.pa[0])*t,c.pa[1]+(c.pb[1]-c.pa[1])*t+Math.sin(Math.PI*t)*40])}c.q=c.p.map(a=>[...a])}
    cab.appendChild(g)}
  if(fresh)for(let i=0;i<160;i++)step();paint();sendCables()}
function step(){for(const c of cables){if(!c.el)continue;const p=c.p,q=c.q,seg=c.L/(N-1);
  for(let i=1;i<N-1;i++){const x=p[i][0],y=p[i][1];p[i]=[x+(x-q[i][0])*.988,y+(y-q[i][1])*.988+.45];q[i]=[x,y]}
  for(let it=0;it<10;it++){p[0]=[...c.pa];p[N-1]=[...c.pb];
    for(let i=0;i<N-1;i++){const a=p[i],b=p[i+1],dx=b[0]-a[0],dy=b[1]-a[1],d=Math.hypot(dx,dy)||1e-3,k=(d-seg)/d,wa=i?1:0,wb=i<N-2?1:0,w=wa+wb;a[0]+=dx*k*wa/w;a[1]+=dy*k*wa/w;b[0]-=dx*k*wb/w;b[1]-=dy*k*wb/w}
    push(c,p)}}}
function jiggle(c,amt){if(!c||!c.q)return;for(let i=1;i<N-1;i++){const s=Math.sin(Math.PI*i/(N-1));c.q[i][0]-=(Math.random()-.5)*amt*s;c.q[i][1]-=amt*s*(.6+Math.random()*.4)}}
function repel(n,x,y,r){const dx=n[0]-x,dy=n[1]-y,d=Math.hypot(dx,dy);if(d<r){const f=(r-d)/(d||1)*.6;n[0]+=(d?dx:0)*f;n[1]+=d?dy*f:r*.6}}
function push(c,p){const g=grab&&cables[grab.i]===c;for(let i=1;i<N-1;i++){const n=p[i];if(hov)repel(n,JACKS[hov].x,JACKS[hov].y,RR);if(ptrIn&&!g)repel(n,mx,my,RP);if(n[1]>VH-8)n[1]=VH-8}}
function paint(){for(const c of cables){if(!c.el)continue;const d=dstr(c.p);c.el.forEach(e=>e.setAttribute("d",d))}}
function pt(e){const r=sv.getBoundingClientRect();mx=(e.clientX-r.left)/r.width*W;my=(e.clientY-r.top)/r.height*VH}
function near(){let b=null,bd=17;for(const g in JACKS){const j=JACKS[g],d=Math.hypot(j.x-mx,j.y-my);if(d<bd){bd=d;b=g}}return b}
const jname=g=>JACKS[g].name;
function setHov(h){hov=h;ring.innerHTML=h?`<circle cx="${JACKS[h].x}" cy="${JACKS[h].y}" r="16" fill="none" stroke="#fff" stroke-opacity=".7" stroke-width="1.5"/>`:"";if(grab)info.textContent="Drop on a jack to plug in · empty space to unplug · Esc to cancel";else if(h)info.textContent=jname(h)+(JACKS[h].dir=="in"?" (input)":" (output)")+" · "+JACKS[h].type}
function plugsAt(j){const o=[];cables.forEach((c,i)=>{if(c.a==j)o.push({i,e:"a"});if(c.b==j)o.push({i,e:"b"})});return o}
function startGrab(i,e,carry){grab={i,e,from:cables[i][e],carry};cables[i][e]=null;build();setHov(hov)}
function newCable(j,carry){cables.push({a:j,b:null,c:cur});grab={i:cables.length-1,e:"b",from:null,carry,isNew:true};build();setHov(hov)}
function legal(a,b){const pa=JACKS[a],pb=JACKS[b];if(pa.dir==pb.dir)return pa.dir=="out"?"Two outputs can't be patched together":"Two inputs can't be patched together";
  const[s,d]=pa.dir=="out"?[pa,pb]:[pb,pa];if(s.type=="Audio"&&d.type=="Gate")return `${d.name} takes a trigger, not audio`;if(s.type=="Gate"&&d.type=="Audio")return `${d.name} takes audio; patch a voice's OUT there`;return null}
function drop(){const t=near(),c=cables[grab.i],other=c[grab.e=="a"?"b":"a"],why=t&&other&&t!=other?legal(t,other):null;
  if(why){if(grab.isNew)cables.splice(grab.i,1);else c[grab.e]=grab.from;grab=null;build();setHov(null);info.textContent=why;return}
  if(t&&t!=other){c[grab.e]=t;cables.splice(grab.i,1);cables.push(c)}else if(t&&!grab.isNew)c[grab.e]=grab.from;else cables.splice(grab.i,1);
  grab=null;build();jiggle(c,6);setHov(near())}
function cancel(){if(!grab)return;if(grab.isNew)cables.splice(grab.i,1);else cables[grab.i][grab.e]=grab.from;grab=null;build();setHov(null)}
function hideMenu(){menu.style.display="none"}
function showMenu(j){const items=plugsAt(j).reverse().map(p=>{const c=cables[p.i],o=c[p.e=="a"?"b":"a"];return `<button data-i="${p.i}" data-e="${p.e}"><i style="background:${COL[c.c][0]}"></i>${o?"to "+jname(o):"loose end"}</button>`}).join("")+`<button data-n="1"><i style="background:${COL[cur][0]};opacity:.5"></i>+ New cable here</button>`;
  menu.innerHTML=`<div style="padding:4px 9px;opacity:.6;font-size:12px">${jname(j)} · top of stack first · click to pick up</div>`+items;
  const r=stage.getBoundingClientRect();menu.style.display="block";const jy=JACKS[j].y/VH*r.height,mh=menu.offsetHeight;menu.style.left=Math.max(4,Math.min(JACKS[j].x/W*r.width,r.width-220))+"px";menu.style.top=Math.max(4,jy+16+mh>r.height?jy-mh-16:jy+16)+"px";menu.dataset.j=j}
menu.addEventListener("click",e=>{const b=e.target.closest("button");if(!b)return;const j=menu.dataset.j;hideMenu();if(b.dataset.n)newCable(j,true);else startGrab(+b.dataset.i,b.dataset.e,true)});
$("clr").onclick=()=>{cables=[];build();info.textContent="Cables cleared"};


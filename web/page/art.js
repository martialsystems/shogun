// ================= panel art =================
const DEFS='<linearGradient id="pf" x1="0" y1="0" x2="0" y2="1"><stop offset="0" stop-color="#181918"/><stop offset="1" stop-color="#0c0d0c"/></linearGradient>'+
 '<radialGradient id="js" cx=".4" cy=".35" r=".8"><stop offset="0" stop-color="#e6e6e1"/><stop offset="1" stop-color="#7d7d79"/></radialGradient><radialGradient id="jn" cx=".4" cy=".35" r=".8"><stop offset="0" stop-color="#9a9a96"/><stop offset="1" stop-color="#3c3c3c"/></radialGradient>'+
 '<linearGradient id="kb" x1="0" y1="0" x2="1" y2="1"><stop offset="0" stop-color="#4b4b4e"/><stop offset=".5" stop-color="#1a1a1b"/><stop offset="1" stop-color="#060607"/></linearGradient><linearGradient id="kt" x1="0" y1="0" x2="1" y2="1"><stop offset="0" stop-color="#2a2a2c"/><stop offset="1" stop-color="#131314"/></linearGradient><radialGradient id="ks"><stop offset="0" stop-color="#fff" stop-opacity=".16"/><stop offset="1" stop-color="#fff" stop-opacity="0"/></radialGradient>'+
 '<linearGradient id="bs" x1="0" y1="0" x2="1" y2="1"><stop offset="0" stop-color="#f4eedc"/><stop offset="1" stop-color="#b3ab94"/></linearGradient><radialGradient id="bd" cx=".45" cy=".4" r=".8"><stop offset="0" stop-color="#f8f3e4"/><stop offset="1" stop-color="#d0c8b2"/></radialGradient>'+
 '<linearGradient id="bk" x1="0" y1="0" x2="1" y2="1"><stop offset="0" stop-color="#4a4a4e"/><stop offset="1" stop-color="#0e0e10"/></linearGradient><radialGradient id="bkd" cx=".45" cy=".4" r=".8"><stop offset="0" stop-color="#36363a"/><stop offset="1" stop-color="#161618"/></radialGradient>'+
 '<linearGradient id="lcd" x1="0" y1="0" x2="0" y2="1"><stop offset="0" stop-color="#8f9a7c"/><stop offset=".5" stop-color="#a6b192"/><stop offset="1" stop-color="#94a083"/></linearGradient>'+
 '<radialGradient id="lg"><stop offset="0" stop-color="#ff4a36" stop-opacity=".55"/><stop offset="1" stop-color="#ff4a36" stop-opacity="0"/></radialGradient><radialGradient id="lgg"><stop offset="0" stop-color="#3fe06a" stop-opacity=".5"/><stop offset="1" stop-color="#3fe06a" stop-opacity="0"/></radialGradient>'+
 '<radialGradient id="sl" cx=".4" cy=".35" r=".8"><stop offset="0" stop-color="#f2f2f2"/><stop offset="1" stop-color="#7a7a7a"/></radialGradient>'+
 '<radialGradient id="mdk"><stop offset="0" stop-color="#000"/><stop offset="1" stop-color="#000" stop-opacity="0"/></radialGradient><radialGradient id="mlt"><stop offset="0" stop-color="#fff"/><stop offset="1" stop-color="#fff" stop-opacity="0"/></radialGradient>'+
 '<radialGradient id="vig" cx=".5" cy=".5" r=".75"><stop offset=".55" stop-color="#000" stop-opacity="0"/><stop offset="1" stop-color="#000" stop-opacity=".38"/></radialGradient>'+
 '<linearGradient id="edgeT" x1="0" y1="0" x2="0" y2="1"><stop offset="0" stop-color="#bdbdb8" stop-opacity=".22"/><stop offset="1" stop-color="#bdbdb8" stop-opacity="0"/></linearGradient><linearGradient id="edgeL" x1="0" y1="0" x2="1" y2="0"><stop offset="0" stop-color="#bdbdb8" stop-opacity=".18"/><stop offset="1" stop-color="#bdbdb8" stop-opacity="0"/></linearGradient>'+
 '<filter id="grain" x="0" y="0" width="100%" height="100%"><feTurbulence type="fractalNoise" baseFrequency=".85" numOctaves="2" seed="7"/><feColorMatrix type="matrix" values="0 0 0 0 .5  0 0 0 0 .5  0 0 0 0 .5  0 0 0 .14 0"/></filter>'+
 '<radialGradient id="screw" cx=".3" cy=".25" r=".9"><stop offset="0" stop-color="#e6e4dc"/><stop offset="1" stop-color="#5d5c58"/></radialGradient>'+
 '<linearGradient id="cabi" x1="0" y1="0" x2="0" y2="1"><stop offset="0" stop-color="#151615"/><stop offset="1" stop-color="#0b0b0c"/></linearGradient>';
function rnd(seed){return()=>{seed|=0;seed=seed+0x6D2B79F5|0;let t=Math.imul(seed^seed>>>15,1|seed);t=t+Math.imul(t^t>>>7,61|t)^t;return((t^t>>>14)>>>0)/4294967296}}
function wear(){const R=rnd(5),u=(a,b)=>a+(b-a)*R();let o="";   // mottling, edge wear, scratches, dust and grain, as on the other panels
  for(let i=0;i<34;i++)o+=`<ellipse cx="${u(0,W)|0}" cy="${u(0,H)|0}" rx="${u(60,320)|0}" ry="${u(30,160)|0}" fill="url(#${R()<.67?"mdk":"mlt"})" opacity="${u(.03,.09).toFixed(3)}"/>`;
  o+=`<rect width="${W}" height="14" fill="url(#edgeT)"/><rect width="12" height="${H}" fill="url(#edgeL)"/><rect x="${W-12}" width="12" height="${H}" fill="url(#edgeL)" transform="rotate(180 ${W-6} ${H/2})"/>`;
  for(let i=0;i<40;i++){const s="tblr"[R()*4|0],x=s=="t"||s=="b"?u(0,W):s=="l"?u(2,10):u(W-10,W-2),y=s=="l"||s=="r"?u(0,H):s=="t"?u(2,10):u(H-10,H-2);o+=`<ellipse cx="${x.toFixed(1)}" cy="${y.toFixed(1)}" rx="${u(1.5,7).toFixed(1)}" ry="${u(1,4).toFixed(1)}" fill="#6a6a66" opacity="${u(.15,.35).toFixed(2)}"/>`}
  for(let i=0;i<70;i++){const x=u(0,W),y=u(0,H),L=u(8,110),a=u(0,6.283);o+=`<line x1="${x.toFixed(1)}" y1="${y.toFixed(1)}" x2="${(x+L*Math.cos(a)).toFixed(1)}" y2="${(y+L*Math.sin(a)).toFixed(1)}" stroke="#dcdcd4" stroke-width="${u(.35,.7).toFixed(2)}" opacity="${u(.05,.16).toFixed(2)}"/>`}
  for(let i=0;i<320;i++)o+=`<circle cx="${u(0,W).toFixed(1)}" cy="${u(0,H).toFixed(1)}" r="${u(.3,.9).toFixed(2)}" fill="#ece8dc" opacity="${u(.2,.55).toFixed(2)}"/>`;
  return o+`<rect width="${W}" height="${H}" fill="none" filter="url(#grain)"/><rect width="${W}" height="${H}" rx="8" fill="url(#vig)"/>`}
function screw(cx,cy){return `<g transform="translate(${cx} ${cy})"><circle r="5" fill="url(#js)" stroke="#000" stroke-width=".9"/><line x1="-3.5" x2="3.5" stroke="#1a1a1a" stroke-width="1.5"/><line y1="-3.5" y2="3.5" stroke="#1a1a1a" stroke-width="1.5"/></g>`}
$("df").innerHTML=DEFS;
const FONTG=a=>`<g font-family="'Liberation Sans',Arial,Helvetica,sans-serif">${a.join("")}</g>`;
$("panel").innerHTML=`<rect width="${W}" height="${H}" rx="8" fill="url(#pf)"/><rect x="3" y="3" width="${W-6}" height="${H-6}" rx="6" fill="none" stroke="#050506" stroke-width="2"/>`+
 FONTG(ART.all)+`<g id="seqArt">${FONTG(ART.seq)}</g><g id="bayArt" style="display:none">${FONTG(ART.bay)}</g>`+
 // the frame, and the heavier rule between the knob block and the bottom half
 `<rect x="${M}" y="${FR_T}" width="${AV}" height="${FR_B-FR_T}" fill="none" stroke="${GRN}" stroke-width="1.6"/><line x1="${M}" y1="${KB_B}" x2="${W-M}" y2="${KB_B}" stroke="${GRN}" stroke-width="3.4"/>`+
 wear()+[[9,9],[W-9,9],[9,H-9],[W-9,H-9]].map(p=>screw(...p)).join("");
// live parts sit above the wear layer, so move them to #live in order
const liveSeq=document.createElementNS(NS,"g");liveSeq.id="liveSeq";
document.querySelectorAll("#panel g[id^=L_]").forEach(g=>{const z=g.closest("#seqArt")?liveSeq:$("live");z.appendChild(g)});$("live").appendChild(liveSeq);


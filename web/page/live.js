// ================= live drawing =================
function knobBody(cx,cy,r,ang){return `<circle cx="${cx+1}" cy="${cy+2}" r="${r+4}" fill="#000" opacity=".45"/><circle cx="${cx}" cy="${cy}" r="${r+3}" fill="#08080a" stroke="#000" stroke-width=".8"/><circle cx="${cx}" cy="${cy}" r="${r+1.2}" fill="none" stroke="#3c3c3f" stroke-width="2.4" stroke-dasharray=".8 1.6" opacity=".75"/><circle cx="${cx}" cy="${cy}" r="${r-.3}" fill="url(#kb)" stroke="#000" stroke-width=".8"/><circle cx="${cx}" cy="${cy}" r="${r*.8}" fill="url(#kt)" stroke="#050505" stroke-width=".8"/><ellipse cx="${cx-r*.28}" cy="${cy-r*.32}" rx="${r*.45}" ry="${r*.28}" fill="url(#ks)" transform="rotate(-35 ${cx} ${cy})"/><line x1="${cx}" y1="${cy-r*.1}" x2="${cx}" y2="${cy-(r-1.5)}" stroke="#f1ede0" stroke-width="2.4" transform="rotate(${ang} ${cx} ${cy})"/>`}
function keyBody(cx,cy,black,down){const o=down?1:0,[k,d,e,s]=black?["bk","bkd","#000","#3a3a3e"]:["bs","bd","#7e7764","#6f6a5a"];
  return `<g transform="translate(${o} ${o})"${down?' opacity=".92"':""}><rect x="${cx-13}" y="${cy-13}" width="26" height="26" rx="3" fill="url(#${k})" stroke="${s}" stroke-width=".9"/><rect x="${cx-13}" y="${cy+9}" width="26" height="4" rx="2" fill="${e}" opacity=".55"/><rect x="${cx-9.5}" y="${cy-10}" width="19" height="17" rx="2.5" fill="url(#${d})"/></g>`}
const LAMPC={r:["#ff4a36","#4a0c08","#ffd2c8","lg"],g:["#3fe06a","#0c3a16","#d2ffdc","lgg"]};
const lamp=(cx,cy,r,on,c="r")=>{const[a,b,h,gl]=LAMPC[c];return(on?`<circle cx="${cx}" cy="${cy}" r="${r*3.4}" fill="url(#${gl})"/>`:"")+`<circle cx="${cx}" cy="${cy}" r="${r}" fill="${on?a:b}"/>`+(on?`<circle cx="${cx-r*.3}" cy="${cy-r*.3}" r="${r*.35}" fill="${h}" opacity=".8"/>`:"")};
const FONT={" ":[0,0,0,0,0,0,0],"0":[14,17,19,21,25,17,14],"1":[4,12,4,4,4,4,14],"2":[14,17,1,2,4,8,31],"3":[31,2,4,2,1,17,14],"4":[2,6,10,18,31,2,2],
"5":[31,16,30,1,1,17,14],"6":[6,8,16,30,17,17,14],"7":[31,1,2,4,8,8,8],"8":[14,17,17,14,17,17,14],"9":[14,17,17,15,1,2,12],
"A":[14,17,17,17,31,17,17],"B":[30,17,17,30,17,17,30],"C":[14,17,16,16,16,17,14],"D":[28,18,17,17,17,18,28],"E":[31,16,16,30,16,16,31],
"F":[31,16,16,30,16,16,16],"G":[14,17,16,23,17,17,15],"H":[17,17,17,31,17,17,17],"I":[14,4,4,4,4,4,14],"J":[7,2,2,2,2,18,12],
"K":[17,18,20,24,20,18,17],"L":[16,16,16,16,16,16,31],"M":[17,27,21,21,17,17,17],"N":[17,17,25,21,19,17,17],"O":[14,17,17,17,17,17,14],
"P":[30,17,17,30,16,16,16],"Q":[14,17,17,17,21,18,13],"R":[30,17,17,30,20,18,17],"S":[15,16,16,14,1,1,30],"T":[31,4,4,4,4,4,4],
"U":[17,17,17,17,17,17,14],"V":[17,17,17,17,17,10,4],"W":[17,17,17,21,21,21,10],"X":[17,17,10,4,10,17,17],"Y":[17,17,17,10,4,4,4],
"Z":[31,1,2,4,8,16,31],"-":[0,0,0,31,0,0,0],"+":[0,4,4,31,4,4,0],"/":[0,1,2,4,8,16,0],".":[0,0,0,0,0,12,12],"'":[4,4,8,0,0,0,0],"*":[0,4,21,14,21,4,0]};
function dots(x,y,w,h,text,n){const p=Math.min(w/(n*6),h/8),d=p*.86,ox=x+(w-n*6*p)/2+p*.5,oy=y+(h-7*p)/2;let g="",l="";
  for(let c=0;c<n;c++){const f=FONT[text[c]||" "]||FONT[" "];for(let r=0;r<7;r++)for(let b=0;b<5;b++){const s=`<rect x="${(ox+(c*6+b)*p).toFixed(2)}" y="${(oy+r*p).toFixed(2)}" width="${d.toFixed(2)}" height="${d.toFixed(2)}"/>`;if((f[r]>>(4-b))&1)l+=s;else g+=s}}
  return `<g fill="#1d2414" opacity=".07">${g}</g><g fill="#1d2414" opacity=".88">${l}</g>`}
const drawn={};

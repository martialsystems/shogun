// Runs build/shogun.wasm for the page. Used inside the AudioWorklet, or on the main thread as the fallback.
// bytes: the wasm file. rate: the audio context rate; the engine runs at that rate (spec v2.2 §15.3, no resampler). post: receives {counter, running} when either changes, and {counter, running, lfo, lfoP}
// (LFO OUT in volts and the LFO's phase) about every 1024 frames.
function shogunHost(bytes,rate,post){
  const x=new WebAssembly.Instance(new WebAssembly.Module(bytes),{env:{sin:Math.sin,cos:Math.cos,tan:Math.tan,exp:Math.exp,exp2:(v)=>Math.pow(2,v),pow:Math.pow,tanh:Math.tanh,log:Math.log,log2:Math.log2,log10:Math.log10,log1p:Math.log1p,atan2:Math.atan2}}).exports;
  x.sg_init(rate);
  const K={};for(let i=0;i<x.sg_knob_count();i++){const m=new Uint8Array(x.memory.buffer);let p=x.sg_knob_name(i),s="";while(m[p])s+=String.fromCharCode(m[p++]);K[s]=i}
  const view=(n)=>[new Float32Array(x.memory.buffer,x.sg_out_l(),n),new Float32Array(x.memory.buffer,x.sg_out_r(),n)];
  const adv=1;let frac=0,oL=0,oR=0,nL=0,nR=0,pos=0,avail=0,bl=null,br=null,last=-2,lastRun=-1,lfoN=0;
  function pull(){if(pos>=avail){x.sg_process(256);[bl,br]=view(256);pos=0;avail=256}nL=bl[pos];nR=br[pos];pos++}
  return{
    run(calls){for(const c of calls){if(c[0]=="knob"){const i=K[c[1]];if(i!=null)x.sg_set_knob(i,c[2])}else if(c[0]=="doc")this.doc(c[1]);else if(typeof x[c[0]]=="function")x[c[0]](...c.slice(1))}},
    // ["doc", text]: a saved pattern's whole document (engine/patch.h), loaded like a factory program
    doc(t){const b=unescape(encodeURIComponent(t)),p=x.sg_doc_buf(b.length),m=new Uint8Array(x.memory.buffer,p,b.length);for(let i=0;i<b.length;i++)m[i]=b.charCodeAt(i);return x.sg_patch_load(b.length)},
    // the engine's whole state as a document (the page's tests compare it with what was saved)
    json(){const p=x.sg_state_json(1),n=x.sg_state_json_len(),m=new Uint8Array(x.memory.buffer,p,n);let b="";for(let i=0;i<n;i++)b+=String.fromCharCode(m[i]);try{return decodeURIComponent(escape(b))}catch(e){return b}},
    render(L,R,n){
      if(Math.abs(adv-1)<1e-9){for(let o=0;o<n;){const k=Math.min(n-o,1024);x.sg_process(k);const[a,b]=view(k);L.set(a,o);if(R!==L)R.set(b,o);o+=k}}
      else for(let i=0;i<n;i++){L[i]=oL+(nL-oL)*frac;if(R!==L)R[i]=oR+(nR-oR)*frac;frac+=adv;while(frac>=1){frac-=1;oL=nL;oR=nR;pull()}}
      const c=x.sg_counter(),r=x.sg_running();lfoN+=n;
      if(lfoN>=1024){lfoN=0;last=c;lastRun=r;post({counter:c,running:!!r,lfo:x.sg_lfo_volts(),lfoP:x.sg_lfo_phase()})}else if(c!==last||r!==lastRun){last=c;lastRun=r;post({counter:c,running:!!r})}}}}

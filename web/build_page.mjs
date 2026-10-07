// Builds web/shogun.html: the panel (web/page/*) with build/shogun.wasm inlined as base64.
// Run after the wasm build: make web
import { readFileSync, writeFileSync } from "node:fs";

const root = new URL("..", import.meta.url).pathname;
const part = (f) => readFileSync(root + "web/page/" + f, "utf8");
const wasm = readFileSync(root + "build/shogun.wasm").toString("base64");
const head = part("head.html").replace("/*__WASM__*/", wasm).replace("/*__HOST__*/", part("host.js"));
const body = ["main1.js", "art.js", "live.js", "main2.js", "cables.js", "main3.js"].map(part).join("\n");
writeFileSync(root + "web/shogun.html", head + body + "\n</script>\n");
console.log("web/shogun.html", Math.round((head.length + body.length) / 1024), "KB");

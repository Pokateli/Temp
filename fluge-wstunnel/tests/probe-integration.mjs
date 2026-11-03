import assert from 'node:assert/strict';
import { probeWssEcho } from '../site-bridge/wss-echo-probe.mjs';
const port = Number(process.argv[2] || 18761);
const r=await probeWssEcho({endpoint:'wss://127.0.0.1:'+port,destinationPort:51820,size:1200});
assert.equal(r.ok,true);
assert.equal(r.packet_bytes,1200);
assert.ok(r.rtt_ms>=0);
console.log('PASS: Site-ready WSS echo probe:',r.rtt_ms,'ms RTT (local mock only)');

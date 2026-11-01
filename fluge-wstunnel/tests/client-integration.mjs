import assert from 'node:assert/strict';
import { openWstunnel, makeWstunnelUpgrade } from '../site-bridge/wstunnel-v11-client.mjs';
const port = Number(process.argv[2] || 18761);
const endpoint = 'ws://127.0.0.1:' + port;
await assert.rejects(makeWstunnelUpgrade({endpoint}), /WSS is required/);
await assert.rejects(makeWstunnelUpgrade({endpoint, allowPlainLoopback: true,
  destinationHost: '192.168.1.1'}), /Destination must/);
const config = await makeWstunnelUpgrade({endpoint, allowPlainLoopback: true});
assert.equal(new URL(config.url).pathname,'/v1/events');
assert.equal(config.protocols[0], 'v1');
assert.match(config.protocols[1], /^authorization\.bearer\.[^.]+\.[^.]+\.[^.]+$/);
const tunnel = await openWstunnel({endpoint, allowPlainLoopback: true});
for (const size of [32, 1200, 4096, 16384]) {
  const input=crypto.getRandomValues(new Uint8Array(size));
  tunnel.send(input);
  const reply=await Promise.race([tunnel.receive(),
    new Promise((_,reject)=>setTimeout(()=>reject(new Error('response timeout')),3000))]);
  assert.deepEqual(reply,input);
}
tunnel.close();
console.log('PASS: compatible Upgrade, JWT metadata, binary WS exchange: 4 payload sizes');

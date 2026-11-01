// Diagnostic only: WSS echo RTT against a dedicated UDP echo port behind wstunnel.
// Do not send random probe bytes to a production WireGuard UDP listener.
import {openWstunnel} from './wstunnel-v11-client.mjs';

export async function probeWssEcho({
  endpoint, destinationPort = 51821, size = 1200, timeoutMs = 3000,
  cryptoAPI = globalThis.crypto, socketFactory,
  allowPlainLoopback = false, now = () => performance.now(),
}) {
  if (!Number.isInteger(size) || size < 1 || size > 60000) throw new Error('Invalid size');
  if (!Number.isInteger(timeoutMs) || timeoutMs < 100 || timeoutMs > 30000) throw new Error('Invalid timeout');
  const tunnel = await openWstunnel({endpoint, destinationPort,
    cryptoAPI, socketFactory, allowPlainLoopback,
    connectTimeoutMs: timeoutMs});
  let timer;
  try {
    const sent = cryptoAPI.getRandomValues(new Uint8Array(size));
    const started = now();
    tunnel.send(sent);
    const reply = await Promise.race([
      tunnel.receive(),
      new Promise((_, reject) => { timer = setTimeout(() => reject(new Error('Echo response timeout')), timeoutMs); }),
    ]);
    const elapsed = now() - started;
    if (reply.length !== sent.length || reply.some((b, i) => b !== sent[i])) {
      throw new Error('Echo integrity mismatch');
    }
    return {
      ok: true, packet_bytes: size, rtt_ms: Math.round(elapsed * 1000) / 1000,
      note: 'WSS-to-UDP echo; excludes ChatGPT-to-MCP call latency',
    };
  } finally {
    clearTimeout(timer);
    tunnel.close();
  }
}

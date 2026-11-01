// wstunnel v11.1.0 WebSocket client handshake adapter.
// No Node-only dependencies. This does not create a TUN or expose MCP as a socket.
const SECRET_PREFIX = 'authorization.bearer.';
const MAX_DATAGRAM = 65507;
const MAX_RECEIVE_QUEUE = 256;
const MAX_SEND_BUFFER_BYTES = 2 * 1024 * 1024;
function base64url(bytes) {
  const alphabet = 'ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_';
  let output = '';
  for (let i = 0; i < bytes.length; i += 3) {
    const a = bytes[i], b = bytes[i + 1], c = bytes[i + 2];
    output += alphabet[a >> 2] + alphabet[((a & 3) << 4) | ((b ?? 0) >> 4)];
    if (i + 1 < bytes.length) output += alphabet[((b & 15) << 2) | ((c ?? 0) >> 6)];
    if (i + 2 < bytes.length) output += alphabet[c & 63];
  }
  return output;
}
async function encodeTunnelJWT(payload, cryptoAPI) {
  const encode = new TextEncoder();
  const header = { alg: 'HS256', typ: 'JWT' };
  const body = [header, payload].map(obj => base64url(encode.encode(JSON.stringify(obj)))).join('.');
  // Upstream v11 decodes routing JWT without signature verification.
  // This randomized HMAC is only wire-format compatibility, NOT authentication.
  const key = await cryptoAPI.subtle.importKey('raw', cryptoAPI.getRandomValues(new Uint8Array(32)),
    { name: 'HMAC', hash: 'SHA-256' }, false, ['sign']);
  const signature = new Uint8Array(await cryptoAPI.subtle.sign('HMAC', key, encode.encode(body)));
  return body + '.' + base64url(signature);
}
export async function makeWstunnelUpgrade({
  endpoint, destinationHost = '127.0.0.1', destinationPort = 51820,
  protocol = 'udp', requestId, cryptoAPI = globalThis.crypto, allowPlainLoopback = false,
}) {
  const url = new URL(endpoint);
  if (!(url.protocol === 'wss:' || (allowPlainLoopback && url.protocol === 'ws:' &&
    (url.hostname === 'localhost' || url.hostname === '127.0.0.1')))) {
    throw new Error('WSS is required except for explicit loopback tests');
  }
  if (url.username || url.password || url.hash || url.search) throw new Error('Unexpected URL credentials, fragment or query');
  if (url.pathname !== '/' && url.pathname !== '/v1/events') throw new Error('Only /v1/events is supported');
  if (destinationHost !== '127.0.0.1' && destinationHost !== 'localhost') {
    throw new Error('Destination must be the restricted WG service on the server loopback');
  }
  if (!Number.isInteger(destinationPort) || destinationPort < 1 || destinationPort > 65535) {
    throw new Error('Invalid destination port');
  }
  if (!['udp', 'tcp'].includes(protocol)) throw new Error('Unsupported protocol');
  if (!cryptoAPI?.randomUUID || !cryptoAPI?.subtle || !cryptoAPI?.getRandomValues) {
    throw new Error('WebCrypto required');
  }
  const payload = {
    id: requestId ?? cryptoAPI.randomUUID(),
    p: protocol === 'udp' ? { Udp: { timeout: null } } : { Tcp: { proxy_protocol: false } },
    r: destinationHost, rp: destinationPort,
  };
  const jwt = await encodeTunnelJWT(payload, cryptoAPI);
  url.pathname = '/v1/events';
  return { url: url.toString(), protocols: ['v1', SECRET_PREFIX + jwt] };
}
export async function openWstunnel({ endpoint, destinationHost, destinationPort, protocol = 'udp',
  socketFactory = (url, protocols) => new WebSocket(url, protocols),
  cryptoAPI = globalThis.crypto, allowPlainLoopback = false, connectTimeoutMs = 8000,
} = {}) {
  const upgrade = await makeWstunnelUpgrade({ endpoint, destinationHost, destinationPort, protocol,
    cryptoAPI, allowPlainLoopback });
  const ws = socketFactory(upgrade.url, upgrade.protocols);
  ws.binaryType = 'arraybuffer';
  const incoming = [], waiters = [];
  let closed = false;
  let closeError = new Error('WebSocket closed');
  const receive = () => {
    if (incoming.length) return Promise.resolve(incoming.shift());
    if (closed) return Promise.reject(closeError);
    return new Promise((resolve, reject) => waiters.push({ resolve, reject }));
  };
  const onMessage = (event) => {
    const data = event.data;
    const promise = data instanceof ArrayBuffer ? Promise.resolve(new Uint8Array(data)) :
      (ArrayBuffer.isView(data) ? Promise.resolve(new Uint8Array(data.buffer, data.byteOffset, data.byteLength)) :
      (typeof data?.arrayBuffer === 'function' ? data.arrayBuffer().then(x => new Uint8Array(x)) :
        Promise.reject(new Error('Nonbinary frame received'))));
    promise.then(packet => {
      if (waiters.length) waiters.shift().resolve(packet);
      else if (incoming.length < MAX_RECEIVE_QUEUE) incoming.push(packet);
      else { closeError = new Error('Receive queue overflow'); ws.close(1009, 'buffer full'); }
    }).catch(err => {
      closeError = err;
      ws.close(1003, 'binary required');
    });
  };
  const onClose = () => {
    closed = true;
    while (waiters.length) waiters.shift().reject(closeError);
  };
  ws.addEventListener('message', onMessage);
  ws.addEventListener('close', onClose);
  ws.addEventListener('error', () => { closeError = new Error('WebSocket transport error'); });
  await new Promise((resolve, reject) => {
    const timer = setTimeout(() => { ws.close(); reject(new Error('WebSocket handshake timeout')); }, connectTimeoutMs);
    ws.addEventListener('open', () => {
      clearTimeout(timer);
      if (ws.protocol !== 'v1') { ws.close(1002, 'subprotocol mismatch'); reject(new Error('Unexpected subprotocol')); }
      else resolve();
    }, { once: true });
    ws.addEventListener('close', () => { clearTimeout(timer); reject(closeError); }, { once: true });
    ws.addEventListener('error', () => { clearTimeout(timer); reject(closeError); }, { once: true });
  });
  return {
    send(packet) {
      if (closed || ws.readyState !== 1) throw new Error('Tunnel not open');
      const bytes = packet instanceof Uint8Array ? packet : new Uint8Array(packet);
      if (!bytes.length || bytes.length > MAX_DATAGRAM) throw new Error('Invalid packet length');
      if (ws.bufferedAmount > MAX_SEND_BUFFER_BYTES) throw new Error('Backpressure: send queue full');
      ws.send(bytes);
    },
    receive,
    close: () => ws.close(),
    get bufferedAmount() { return ws.bufferedAmount; },
  };
}

# Site-compatible WSS client — Флюгегехаймен

## Status

This directory contains a transport client matching the inspected WebSocket handshake of erebe/wstunnel v11.1.0, plus an echo diagnostic. It is NOT automatically deployed to the published Site. It does NOT create a network interface or socket inside the ChatGPT sandbox.

Files:
- wstunnel-v11-client.mjs: Upgrade path, subprotocols, routing JWT, binary frames, finite queues and send-buffer backpressure.
- wss-echo-probe.mjs: checks payload integrity and calculates WSS-UDP echo RTT.
- ../host/udp-echo.py: disposable loopback UDP echo server.
- ../tests/run-local.sh: localhost TLS integration with a mock WebSocket server.

The local test requires Node.js 22+, Python websockets >=14, OpenSSL. Run: bash tests/run-local.sh

## Host lab

Run UDP echo on the host, bound exclusively to 127.0.0.1:51821:
python3 host/udp-echo.py --port 51821

Start a dedicated wstunnel server (use your own trusted TLS cert):
wstunnel server --tls-certificate /etc/fluge/tls/fullchain.pem --tls-private-key /etc/fluge/tls/privkey.pem --restrict-to 127.0.0.1:51821 --restrict-http-upgrade-path-prefix YOUR_RANDOM_SECRET wss://0.0.0.0:443

Generate YOUR_RANDOM_SECRET privately, e.g. openssl rand -hex 32, and store in Site secrets, NEVER in GitHub. This restricted URL path is not a substitute for proper client authentication; paths may leak to logs. Use origin-level authorization / controls whenever available. Never open arbitrary destinations via the server. On production WG-over-WSS, use the real localhost UDP WG listener and the upstream client; do not send random echo probes to WireGuard.

## Connecting in a Site MCP handler

After registering an actual tool through the Site platform's supported API, invoke the exported probeWssEcho function with:
- endpoint: fixed Site secret/config, e.g. wss://your-host.example:443
- pathPrefix: owner-defined secret, matching the server restriction
- destinationPort: 51821
- size: 1200
- timeoutMs: 3000

Do not let MCP callers select arbitrary WSS origins or destinations (SSRF/open proxy). Keep TLS verification enabled. JS WebSocket constructors may not expose mTLS client certificate configuration; use a runtime-supported authenticated origin/proxy where necessary.

No actual Site MCP tool registration or host deployment has occurred in this repository.

## Verified protocol from upstream source

- GET /<path_prefix>/events
- Sec-WebSocket-Protocol: v1, authorization.bearer.<JWT>
- JWT payload: id (UUID), p (protocol), r (host), rp (port). Upstream server uses insecure_decode for routing JWT; JWT is not client authentication.
- A binary WebSocket message conveys a forwarded UDP datagram.

Sources:
https://github.com/erebe/wstunnel/blob/v11.1.0/wstunnel/src/tunnel/transport/websocket.rs
https://github.com/erebe/wstunnel/blob/v11.1.0/wstunnel/src/tunnel/transport/jwt.rs
https://github.com/erebe/wstunnel/blob/v11.1.0/wstunnel/src/tunnel/server/handler_websocket.rs

## Fundamental limitation

ChatGPT MCP tool invocations are discrete. They do not give a sandbox process a persistent socket or TUN link. A fast Site-to-Host WSS connection must not be described as direct ChatGPT-sandbox TCP/IP.

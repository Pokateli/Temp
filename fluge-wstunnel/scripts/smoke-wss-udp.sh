#!/usr/bin/env bash
# Local-only functional test: UDP echo -> wstunnel client -> WSS -> wstunnel server -> UDP echo.
set -euo pipefail

WSTUNNEL_BIN="${WSTUNNEL_BIN:-wstunnel}"
WSS_PORT="${WSS_PORT:-18443}"
LOCAL_UDP_PORT="${LOCAL_UDP_PORT:-15182}"
ECHO_UDP_PORT="${ECHO_UDP_PORT:-15183}"

for cmd in openssl python3 mktemp; do
  command -v "$cmd" >/dev/null || { echo "Missing dependency: $cmd" >&2; exit 2; }
done
command -v "$WSTUNNEL_BIN" >/dev/null || { echo "wstunnel not found: $WSTUNNEL_BIN" >&2; exit 2; }

TMP="$(mktemp -d)"
echo_pid=''
server_pid=''
client_pid=''
cleanup() {
  for p in "$client_pid" "$server_pid" "$echo_pid"; do
    if [[ -n "$p" ]]; then
      kill "$p" 2>/dev/null || true
      wait "$p" 2>/dev/null || true
    fi
  done
  rm -rf "$TMP"
}
trap cleanup EXIT

openssl req -new -x509 -newkey rsa:2048 -sha256 -nodes -days 1 \
  -subj '/CN=localhost' \
  -keyout "$TMP/key.pem" -out "$TMP/cert.pem" >/dev/null 2>&1

python3 -u - "$ECHO_UDP_PORT" >"$TMP/echo.log" 2>&1 <<'PY' &
import socket, sys
s=socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
s.bind(('127.0.0.1', int(sys.argv[1])))
while True:
    payload, peer=s.recvfrom(65535)
    s.sendto(payload,peer)
PY
echo_pid="$!"

"$WSTUNNEL_BIN" server \
  --restrict-to "127.0.0.1:$ECHO_UDP_PORT" \
  --tls-certificate "$TMP/cert.pem" \
  --tls-private-key "$TMP/key.pem" \
  "wss://127.0.0.1:$WSS_PORT" >"$TMP/server.log" 2>&1 &
server_pid="$!"

sleep 1
"$WSTUNNEL_BIN" client \
  -L "udp://127.0.0.1:$LOCAL_UDP_PORT:127.0.0.1:$ECHO_UDP_PORT?timeout_sec=0" \
  "wss://127.0.0.1:$WSS_PORT" >"$TMP/client.log" 2>&1 &
client_pid="$!"

if python3 - "$LOCAL_UDP_PORT" <<'PY'
import socket, sys, time
addr=('127.0.0.1', int(sys.argv[1]))
payload=b'fluge:wss:udp:echo'
s=socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
s.settimeout(0.7)
for i in range(18):
    try:
        s.sendto(payload, addr)
        reply,_=s.recvfrom(65535)
        if reply==payload:
            print('PASS: WSS UDP echo; roundtrip binary payload OK')
            sys.exit(0)
    except socket.timeout:
        pass
    time.sleep(0.25)
print('FAIL: WSS UDP echo did not return', file=sys.stderr)
sys.exit(1)
PY
then
  echo "Test complete (local only; ChatGPT↔MCP transport NOT tested)."
else
  echo '--- server.log ---' >&2
  tail -n 40 "$TMP/server.log" >&2
  echo '--- client.log ---' >&2
  tail -n 40 "$TMP/client.log" >&2
  echo '--- echo.log ---' >&2
  tail -n 20 "$TMP/echo.log" >&2
  exit 1
fi

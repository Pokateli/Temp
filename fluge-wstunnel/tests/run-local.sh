#!/usr/bin/env bash
# Local WSS integration using an explicitly trusted ephemeral TLS certificate.
set -euo pipefail
BASE="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PORT="${WSS_TEST_PORT:-18761}"
for cmd in node python3 openssl mktemp; do command -v "$cmd" >/dev/null || { echo "Missing $cmd" >&2; exit 2; }; done
python3 -c 'import websockets; assert tuple(map(int,websockets.__version__.split(".")[:1])) >= (14,)' || { echo 'Python websockets 14+ required' >&2; exit 2; }
TMP="$(mktemp -d)"
server_pid=''
cleanup() {
  if [[ -n "$server_pid" ]]; then kill "$server_pid" 2>/dev/null || true; wait "$server_pid" 2>/dev/null || true; fi
  rm -rf "$TMP"
}
trap cleanup EXIT
openssl req -new -x509 -newkey rsa:2048 -sha256 -nodes -days 1 \
  -subj '/CN=127.0.0.1' -addext 'subjectAltName=IP:127.0.0.1' \
  -keyout "$TMP/key.pem" -out "$TMP/cert.pem" >/dev/null 2>&1
MOCK_WSS_CERT="$TMP/cert.pem" MOCK_WSS_KEY="$TMP/key.pem" \
  python3 -u "$BASE/tests/mock-wstunnel-server.py" "$PORT" >"$TMP/server.log" 2>&1 &
server_pid="$!"
for _ in $(seq 1 50); do
  if grep -q '^READY$' "$TMP/server.log"; then break; fi
  if ! kill -0 "$server_pid" 2>/dev/null; then cat "$TMP/server.log" >&2; exit 1; fi
  sleep 0.1
done
if ! grep -q '^READY$' "$TMP/server.log"; then echo 'WSS mock failed to start' >&2; cat "$TMP/server.log" >&2; exit 1; fi
NODE_EXTRA_CA_CERTS="$TMP/cert.pem" node "$BASE/tests/client-integration.mjs" "$PORT" --wss
NODE_EXTRA_CA_CERTS="$TMP/cert.pem" node "$BASE/tests/probe-integration.mjs" "$PORT"

#!/usr/bin/env python3
"""Loopback-only WS test double, NOT the real upstream server."""
import asyncio
import base64
import json
import sys
import websockets

def decode_header(headers):
    protocols = headers.get('Sec-WebSocket-Protocol', '')
    values = [item.strip() for item in protocols.split(',')]
    assert 'v1' in values, values
    token = next(x.removeprefix('authorization.bearer.') for x in values if x.startswith('authorization.bearer.'))
    payload = token.split('.')[1]
    return json.loads(base64.urlsafe_b64decode(payload + '=' * (-len(payload) % 4)))

async def handler(ws):
    assert ws.request.path == '/v1/events', ws.request.path
    payload = decode_header(ws.request.headers)
    assert payload['p'] == {'Udp': {'timeout': None}}, payload
    assert payload['r'] == '127.0.0.1', payload
    assert payload['rp'] == 51820, payload
    assert ws.subprotocol == 'v1', ws.subprotocol
    async for message in ws:
        assert isinstance(message, bytes)
        await ws.send(message)

async def main():
    port = int(sys.argv[1])
    async with websockets.serve(handler, '127.0.0.1', port, subprotocols=['v1']):
        print('READY', flush=True)
        await asyncio.Event().wait()

if __name__ == '__main__':
    asyncio.run(main())

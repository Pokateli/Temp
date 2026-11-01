"""Authenticated WSS gateway: allowlisted TCP + fixed-destination UDP.

Deploy behind a TLS terminating HTTPS proxy or provide --tls-cert/--tls-key.
This gateway does not provide a public open proxy. SOCKS requests are denied
unless their exact destination is listed with --allow-tcp.
"""
import argparse
import asyncio
import hmac
import json
import logging
import os
import socket
import ssl

from websockets.asyncio.server import serve
from .protocol import Kind, MAX_PAYLOAD, pack, unpack

log = logging.getLogger('sandboxvpn.gateway')


def hostport(spec: str):
    if spec.startswith('['):
        end = spec.find(']')
        if end < 0 or spec[end + 1:end + 2] != ':':
            raise ValueError('invalid [ipv6]:port')
        host, port = spec[1:end], spec[end + 2:]
    else:
        host, sep, port = spec.rpartition(':')
        if not sep:
            raise ValueError('use host:port')
    if not host or not port.isdecimal() or not 1 <= int(port) <= 65535:
        raise ValueError('invalid host or port')
    return host, int(port)


class ReplyProtocol(asyncio.DatagramProtocol):
    def __init__(self, gateway, channel):
        self.gateway = gateway
        self.channel = channel

    def datagram_received(self, data, addr):
        asyncio.create_task(self.gateway.send(Kind.UDP_REPLY, self.channel, data))

    def error_received(self, err):
        log.warning('UDP upstream error: %s', err)


class GatewaySession:
    def __init__(self, ws, allowed, udp_target, max_channels=128):
        self.ws = ws
        self.allowed = allowed
        self.udp_target = udp_target
        self.max_channels = max_channels
        self.tcp = {}  # id -> (reader, writer, pump task)
        self.udp = {}  # id -> datagram transport
        self.tasks = set()
        self.send_lock = asyncio.Lock()

    async def send(self, kind, channel, data=b''):
        try:
            async with self.send_lock:
                await self.ws.send(pack(kind, channel, data))
        except Exception:
            pass

    async def open_tcp(self, cid, data):
        if cid in self.tcp or cid in self.udp or len(self.tcp) + len(self.udp) >= self.max_channels:
            await self.send(Kind.TCP_ERROR, cid, b'channel limit/duplicate')
            return
        try:
            params = json.loads(data)
            host, port = params['host'], params['port']
            if not isinstance(host, str) or not isinstance(port, int) or (host, port) not in self.allowed:
                raise ValueError('target not in gateway TCP allowlist')
            reader, writer = await asyncio.wait_for(asyncio.open_connection(host, port), timeout=7)
        except (ValueError, KeyError, TypeError, OSError, asyncio.TimeoutError) as exc:
            await self.send(Kind.TCP_ERROR, cid, str(exc).encode()[:240])
            return
        task = asyncio.create_task(self.pump_tcp(cid, reader, writer))
        self.tcp[cid] = (reader, writer, task)
        await self.send(Kind.TCP_OK, cid)

    async def pump_tcp(self, cid, reader, writer):
        try:
            while True:
                data = await reader.read(32768)
                if not data:
                    break
                await self.send(Kind.TCP_DATA, cid, data)
        except (OSError, asyncio.CancelledError):
            pass
        finally:
            await self.send(Kind.TCP_CLOSE, cid)
            writer.close()
            self.tcp.pop(cid, None)

    async def send_udp(self, cid, payload):
        if self.udp_target is None:
            return
        transport = self.udp.get(cid)
        if transport is None:
            if cid in self.tcp or len(self.tcp) + len(self.udp) >= self.max_channels:
                return
            try:
                transport, _ = await asyncio.get_running_loop().create_datagram_endpoint(
                    lambda: ReplyProtocol(self, cid), remote_addr=self.udp_target)
            except OSError as exc:
                log.warning('UDP transport refused: %s', exc)
                return
            self.udp[cid] = transport
        transport.sendto(payload)

    async def handle(self, frame):
        kind, cid, payload = unpack(frame)
        if kind == Kind.TCP_OPEN:
            task = asyncio.create_task(self.open_tcp(cid, payload))
            self.tasks.add(task)
            task.add_done_callback(self.tasks.discard)
        elif kind == Kind.TCP_DATA:
            ch = self.tcp.get(cid)
            if ch:
                _, writer, _ = ch
                writer.write(payload)
                await writer.drain()
        elif kind == Kind.TCP_CLOSE:
            ch = self.tcp.get(cid)
            if ch:
                _, writer, _ = ch
                if writer.can_write_eof():
                    writer.write_eof()
                else:
                    writer.close()
        elif kind == Kind.UDP_DATA:
            await self.send_udp(cid, payload)

    async def close(self):
        for task in list(self.tasks):
            task.cancel()
        for _, writer, task in list(self.tcp.values()):
            task.cancel()
            writer.close()
        for transport in list(self.udp.values()):
            transport.close()
        self.tcp.clear()
        self.udp.clear()


async def start_gateway(listen='127.0.0.1', port=8765, token=None, tcp_targets=(), udp_target=None, ssl_context=None, max_channels=128):
    if not token or len(token) < 32:
        raise ValueError('set SANDBOXVPN_TOKEN with at least 32 characters')
    allowed = frozenset(tcp_targets)

    async def accept(ws):
        auth = ws.request.headers.get('Authorization', '')
        if not hmac.compare_digest(auth, 'Bearer ' + token):
            await ws.close(code=1008, reason='unauthorized')
            return
        session = GatewaySession(ws, allowed, udp_target, max_channels)
        try:
            async for frame in ws:
                try:
                    await session.handle(frame)
                except (ValueError, TypeError, OSError) as exc:
                    log.warning('malformed frame: %s', exc)
                    await ws.close(code=1002, reason='invalid frame')
                    return
        finally:
            await session.close()

    return await serve(accept, listen, port, ssl=ssl_context,
                       max_size=MAX_PAYLOAD + 9, max_queue=32, ping_interval=20, ping_timeout=20)


def main():
    ap = argparse.ArgumentParser(description='SandboxVPN WSS Gateway')
    ap.add_argument('--listen', default='127.0.0.1')
    ap.add_argument('--port', type=int, default=8765)
    ap.add_argument('--allow-tcp', action='append', default=[], metavar='HOST:PORT')
    ap.add_argument('--udp-target', help='fixed UDP destination, e.g. 37.110.89.86:6269')
    ap.add_argument('--tls-cert'); ap.add_argument('--tls-key')
    ap.add_argument('--allow-ws-local', action='store_true', help='only for 127.0.0.1 reverse proxy / lab')
    ap.add_argument('--max-channels', type=int, default=128)
    args = ap.parse_args()
    if not args.tls_cert and not (args.allow_ws_local and args.listen in ('127.0.0.1', '::1', 'localhost')):
        ap.error('provide TLS cert/key, or explicitly use --allow-ws-local on loopback')
    if bool(args.tls_cert) != bool(args.tls_key):
        ap.error('TLS needs both --tls-cert and --tls-key')
    ctx = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER) if args.tls_cert else None
    if ctx:ctx.load_cert_chain(args.tls_cert,args.tls_key)
    token=os.environ.get('SANDBOXVPN_TOKEN', '')
    async def runner():
        srv=await start_gateway(args.listen,args.port,token, [hostport(x) for x in args.allow_tcp],
                                hostport(args.udp_target) if args.udp_target else None,ctx,args.max_channels)
        log.info('gateway listening on %s:%s',args.listen,args.port)
        async with srv: await asyncio.Future()
    logging.basicConfig(level=logging.INFO)
    asyncio.run(runner())

if __name__=='__main__':main()


"""Userspace sandbox transport: SOCKS5 TCP and local UDP over multiplexed WSS.

No TUN, no CAP_NET_ADMIN and no kernel routes required. This is NOT an L3 VPN.
"""
import argparse
import asyncio
import ipaddress
import json
import logging
import os
import ssl
import struct
from urllib.parse import urlsplit

from websockets.asyncio.client import connect
from .protocol import Kind, MAX_PAYLOAD, pack, unpack

log = logging.getLogger('sandboxvpn.client')


class LocalUDP(asyncio.DatagramProtocol):
    def __init__(self, client):
        self.client = client

    def datagram_received(self, data, source):
        self.client.local_datagram(data, source)


class SandboxClient:
    def __init__(self, url, token, socks_port=1080, udp_port=51820, ca_file=None, allow_ws_local=False):
        if len(token) < 32:
            raise ValueError('token must be at least 32 characters')
        u = urlsplit(url)
        if u.scheme != 'wss':
            if not (allow_ws_local and u.scheme == 'ws' and u.hostname in ('localhost', '127.0.0.1', '::1')):
                raise ValueError('use wss:// (ws:// is allowed only for local testing)')
        self.url, self.token, self.socks_port, self.udp_port = url, token, socks_port, udp_port
        self.ca_file = ca_file
        self.ws = None
        self.lock = asyncio.Lock()
        self.next_id = 1
        self.pending = {}
        self.tcp = {}
        self.udp_sources = {}
        self.udp_channels = {}
        self.udp_transport = None
        self.tcp_server = None
        self.ready = asyncio.Event()

    def channel(self):
        cid = self.next_id
        self.next_id = (self.next_id % (2**32 - 1)) + 1
        if cid in self.tcp or cid in self.udp_channels or cid in self.pending:
            raise RuntimeError('channel id collision')
        return cid

    async def send(self, kind, cid, payload=b''):
        if self.ws is None:
            raise ConnectionError('tunnel offline')
        async with self.lock:
            await self.ws.send(pack(kind, cid, payload))

    async def handle_frame(self, frame):
        kind, cid, payload = unpack(frame)
        if kind in (Kind.TCP_OK, Kind.TCP_ERROR):
            fut = self.pending.get(cid)
            if fut is not None and not fut.done():
                if kind == Kind.TCP_OK:
                    fut.set_result(None)
                else:
                    fut.set_exception(ConnectionError(payload.decode('utf-8','replace')))
        elif kind == Kind.TCP_DATA:
            writer = self.tcp.get(cid)
            if writer:
                writer.write(payload)
                await writer.drain()
        elif kind == Kind.TCP_CLOSE:
            writer = self.tcp.pop(cid, None)
            if writer:
                writer.close()
        elif kind == Kind.UDP_REPLY:
            source = self.udp_channels.get(cid)
            if source is not None and self.udp_transport is not None:
                self.udp_transport.sendto(payload, source)

    def local_datagram(self, data, source):
        if self.ws is None or len(data) > MAX_PAYLOAD:
            return
        cid = self.udp_sources.get(source)
        if cid is None:
            cid = self.channel()
            self.udp_sources[source] = cid
            self.udp_channels[cid] = source
        task = asyncio.create_task(self.send(Kind.UDP_DATA, cid, bytes(data)))
        task.add_done_callback(lambda t: t.exception() if not t.cancelled() else None)

    async def socks_connect(self, reader, writer):
        cid = None
        try:
            greeting = await asyncio.wait_for(reader.readexactly(2), timeout=10)
            ver, nmethods = greeting
            if ver != 5 or not 1 <= nmethods <= 32: return
            methods = await reader.readexactly(nmethods)
            if 0 not in methods:
                writer.write(b'\x05\xff'); await writer.drain(); return
            writer.write(b'\x05\x00'); await writer.drain()
            version, command, _, address_type = await reader.readexactly(4)
            if version != 5 or command != 1: raise ValueError('only SOCKS5 CONNECT is supported')
            if address_type == 1:
                host = str(ipaddress.IPv4Address(await reader.readexactly(4)))
            elif address_type == 4:
                host = str(ipaddress.IPv6Address(await reader.readexactly(16)))
            elif address_type == 3:
                host_len = (await reader.readexactly(1))[0]
                if host_len == 0:return
                host = (await reader.readexactly(host_len)).decode('idna')
            else: raise ValueError('invalid SOCKS address type')
            port, = struct.unpack('!H', await reader.readexactly(2))
            if self.ws is None:raise ConnectionError('tunnel disconnected')
            cid = self.channel()
            loop = asyncio.get_running_loop()
            future = loop.create_future()
            self.pending[cid] = future
            await self.send(Kind.TCP_OPEN, cid, json.dumps({'host':host,'port':port}).encode())
            await asyncio.wait_for(future, timeout=10)
            self.tcp[cid] = writer
            writer.write(b'\x05\x00\x00\x01\x00\x00\x00\x00\x00\x00')
            await writer.drain()
            while True:
                data = await reader.read(32768)
                if not data:break
                await self.send(Kind.TCP_DATA, cid, data)
        except (asyncio.IncompleteReadError, asyncio.TimeoutError, ConnectionError, OSError, ValueError) as exc:
            log.debug('SOCKS rejected: %s',exc)
            if cid is not None and cid not in self.tcp:
                try:writer.write(b'\x05\x05\x00\x01\x00\x00\x00\x00\x00\x00');await writer.drain()
                except OSError:pass
        finally:
            if cid is not None:
                self.pending.pop(cid,None)
                if self.ws is not None:
                    try:await self.send(Kind.TCP_CLOSE,cid)
                    except (ConnectionError, OSError):pass
                self.tcp.pop(cid,None)
            writer.close()

    async def disconnect(self):
        self.ready.clear()
        self.ws = None
        for fut in self.pending.values():
            if not fut.done():fut.set_exception(ConnectionError('disconnected'))
        self.pending.clear()
        for writer in self.tcp.values():
            writer.close()
        self.tcp.clear()
        self.udp_sources.clear()
        self.udp_channels.clear()

    async def run(self):
        loop = asyncio.get_running_loop()
        self.tcp_server = await asyncio.start_server(self.socks_connect,'127.0.0.1',self.socks_port)
        self.udp_transport, _ = await loop.create_datagram_endpoint(lambda:LocalUDP(self),local_addr=('127.0.0.1',self.udp_port))
        log.info('SOCKS5 tcp://127.0.0.1:%s / WG-AWG UDP 127.0.0.1:%s',self.socks_port,self.udp_port)
        backoff = 0.5
        ctx = ssl.create_default_context(cafile=self.ca_file) if self.url.startswith('wss:') else None
        try:
            while True:
                try:
                    async with connect(self.url,additional_headers={'Authorization':'Bearer '+self.token},
                                       ssl=ctx, max_size=MAX_PAYLOAD+9,max_queue=32,
                                       ping_interval=20,ping_timeout=20,open_timeout=7) as ws:
                        self.ws=ws
                        self.ready.set()
                        log.info('WSS session established')
                        backoff=0.5
                        async for frame in ws:
                            await self.handle_frame(frame)
                except asyncio.CancelledError:
                    raise
                except Exception as exc:
                    log.warning('tunnel offline: %s',str(exc)[:150])
                finally:
                    await self.disconnect()
                await asyncio.sleep(backoff)
                backoff=min(backoff*2,20)
        finally:
            self.udp_transport.close()
            self.tcp_server.close()
            await self.tcp_server.wait_closed()


def main():
    ap=argparse.ArgumentParser(description='SandboxVPN client (userspace; no TUN)')
    ap.add_argument('--url',required=True,help='wss://gateway.example/vpn')
    ap.add_argument('--socks-port',type=int,default=1080)
    ap.add_argument('--udp-port',type=int,default=51820)
    ap.add_argument('--ca-file',help='trusted CA PEM for private WSS gateway')
    ap.add_argument('--allow-ws-local',action='store_true',help='testing only, loopback ws://')
    args=ap.parse_args()
    logging.basicConfig(level=logging.INFO)
    client=SandboxClient(args.url,os.environ.get('SANDBOXVPN_TOKEN',''),args.socks_port,args.udp_port,
                         args.ca_file,args.allow_ws_local)
    asyncio.run(client.run())

if __name__=='__main__':main()


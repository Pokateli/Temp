import asyncio
import socket
import struct

import pytest
from sandboxvpn.gateway import start_gateway
from sandboxvpn.client import SandboxClient

TOKEN = 'testing-secret-32-characters-long-at-least'

class UDPEcho(asyncio.DatagramProtocol):
    def connection_made(self, transport):self.transport=transport
    def datagram_received(self, data,addr):self.transport.sendto(data[::-1],addr)

async def echo(reader,writer):
    try:
        while True:
            data=await reader.read(4096)
            if not data:break
            writer.write(data[::-1]); await writer.drain()
    finally:writer.close()

async def socks_connect(port,target_port):
    reader,writer=await asyncio.open_connection('127.0.0.1',port)
    writer.write(b'\x05\x01\x00');await writer.drain()
    assert await reader.readexactly(2)==b'\x05\x00'
    writer.write(b'\x05\x01\x00\x01'+socket.inet_aton('127.0.0.1')+struct.pack('!H',target_port))
    await writer.drain()
    return reader,writer,await reader.readexactly(10)

@pytest.mark.asyncio
async def test_tcp_udp_and_rejected_target(unused_tcp_port_factory):
    tcp=await asyncio.start_server(echo,'127.0.0.1',0)
    tp=tcp.sockets[0].getsockname()[1]
    udp, _ = await asyncio.get_running_loop().create_datagram_endpoint(UDPEcho,local_addr=('127.0.0.1',0))
    up=udp.get_extra_info('sockname')[1]
    gate=await start_gateway(port=0,token=TOKEN,tcp_targets={('127.0.0.1',tp)},udp_target=('127.0.0.1',up))
    gp=gate.sockets[0].getsockname()[1]
    sp=unused_tcp_port_factory(); cp=unused_tcp_port_factory()
    client=SandboxClient(f'ws://127.0.0.1:{gp}',TOKEN,sp,cp,allow_ws_local=True)
    task=asyncio.create_task(client.run())
    try:
        await asyncio.wait_for(client.ready.wait(),5)
        rd,wr,ack=await socks_connect(sp,tp)
        assert ack[:2]==b'\x05\x00'
        wr.write(b'HELLO');await wr.drain()
        assert await asyncio.wait_for(rd.readexactly(5),3)==b'OLLEH'
        wr.close();await wr.wait_closed()
        rd,wr,ack=await socks_connect(sp,tp+1)
        assert ack[:2]!=b'\x05\x00'
        wr.close()
        loop=asyncio.get_running_loop()
        received=loop.create_future()
        class User(asyncio.DatagramProtocol):
            def datagram_received(self,data,addr):
                if not received.done():received.set_result(data)
        user,_=await loop.create_datagram_endpoint(User,local_addr=('127.0.0.1',0))
        try:
            user.sendto(b'wireguard-test',('127.0.0.1',cp))
            assert await asyncio.wait_for(received,3)==b'tset-draugeriw'
        finally:user.close()
    finally:
        task.cancel()
        try:await task
        except asyncio.CancelledError:pass
        gate.close();await gate.wait_closed()
        tcp.close();await tcp.wait_closed();udp.close()

@pytest.mark.asyncio
async def test_reconnect(unused_tcp_port_factory):
    gateway=await start_gateway(port=0,token=TOKEN)
    port=gateway.sockets[0].getsockname()[1]
    sp=unused_tcp_port_factory(); up=unused_tcp_port_factory()
    client=SandboxClient(f'ws://127.0.0.1:{port}',TOKEN,sp,up,allow_ws_local=True)
    task=asyncio.create_task(client.run())
    try:
        await asyncio.wait_for(client.ready.wait(),5)
        # Force a network drop, then verify the client reconnects automatically.
        active=client.ws
        await active.close(code=1001, reason='test disconnect')
        await asyncio.wait_for(_wait_until(lambda: client.ws is not None and client.ws is not active),5)
        assert client.ready.is_set()
    finally:
        task.cancel()
        try:await task
        except asyncio.CancelledError:pass
        gateway.close();await gateway.wait_closed()

async def _wait_until(predicate,timeout_step=0.02):
    while not predicate():
        await asyncio.sleep(timeout_step)


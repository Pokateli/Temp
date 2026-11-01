import asyncio
import ssl
import subprocess
import tempfile
from pathlib import Path

import pytest
from websockets.asyncio.client import connect
from websockets.exceptions import ConnectionClosedError

from sandboxvpn.gateway import start_gateway
from sandboxvpn.client import SandboxClient
from sandboxvpn.protocol import Kind, pack

TOKEN='another-fully-secure-testing-token-1234567'

@pytest.mark.asyncio
async def test_auth_mandatory():
    gateway=await start_gateway(port=0,token=TOKEN)
    port=gateway.sockets[0].getsockname()[1]
    try:
        async with connect(f'ws://127.0.0.1:{port}') as ws:
            with pytest.raises(ConnectionClosedError) as ex:
                await ws.recv()
            assert ex.value.rcvd.code == 1008
    finally:
        gateway.close();await gateway.wait_closed()

@pytest.mark.asyncio
async def test_non_loopback_plain_ws_rejected():
    with pytest.raises(ValueError):
        SandboxClient('ws://anotherhost.example:8765','x'*32)

@pytest.mark.asyncio
async def test_tls_validated():
    with tempfile.TemporaryDirectory() as folder:
        cert=Path(folder)/'cert.pem'; key=Path(folder)/'key.pem'
        subprocess.run(['openssl','req','-x509','-newkey','rsa:2048','-nodes',
                        '-keyout',str(key),'-out',str(cert),'-days','1',
                        '-subj','/CN=localhost','-addext','subjectAltName=DNS:localhost'],
                       check=True,capture_output=True)
        sctx=ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
        sctx.load_cert_chain(str(cert),str(key))
        gateway=await start_gateway(port=0,token=TOKEN,ssl_context=sctx)
        port=gateway.sockets[0].getsockname()[1]
        try:
            cctx=ssl.create_default_context(cafile=str(cert))
            async with connect(f'wss://localhost:{port}',ssl=cctx,
                               additional_headers={'Authorization':'Bearer '+TOKEN}) as ws:
                await ws.send(pack(Kind.TCP_OPEN,11,b'{"host":"127.0.0.1","port":12345}'))
                reply=await asyncio.wait_for(ws.recv(),2)
                assert reply[0]==Kind.TCP_ERROR
            with pytest.raises(ssl.SSLCertVerificationError):
                async with connect(f'wss://localhost:{port}',ssl=ssl.create_default_context()):pass
        finally:
            gateway.close();await gateway.wait_closed()


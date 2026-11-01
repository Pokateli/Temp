"""Experimental manually driven MCP datagram handoff.

This is NOT a continuous bridge. The ChatGPT model must relay each printed
packet via its MCP transport_exchange tool and provide any returned bytes.
No model tool can be invoked directly by a Python sandbox process.
"""
import argparse
import asyncio
import base64
import json
import logging
import sys
import time
from .protocol import MAX_PAYLOAD

log = logging.getLogger('sandboxvpn.mcp_handoff')

class Handoff(asyncio.DatagramProtocol):
    def __init__(self):
        self.transport = None
        self.sequence = 0
        self.requests = {}  # seq -> (address, timestamp)
        self.queue = asyncio.Queue(maxsize=64)

    def connection_made(self, transport):
        self.transport = transport

    def datagram_received(self, data, addr):
        now=time.monotonic()
        self.requests={k:v for k,v in self.requests.items() if now-v[1]<60}
        if len(data)>MAX_PAYLOAD or self.queue.full() or len(self.requests)>=64:return
        self.sequence += 1
        seq=self.sequence
        self.requests[seq] = (addr, now)
        self.queue.put_nowait({'seq':seq,'tool':'transport_exchange',
                               'payload_base64':base64.b64encode(data).decode('ascii')})

    async def printer(self):
        while True:
            item=await self.queue.get()
            print(json.dumps(item, separators=(',',':')), flush=True)

    async def reader(self):
        # Driven by JSON lines from a human/operator, not connected to the MCP runtime.
        while True:
            line=await asyncio.to_thread(sys.stdin.readline)
            if not line:return
            try:
                response=json.loads(line)
                seq=response['seq']
                request=self.requests.pop(seq, None)
                data=base64.b64decode(response.get('response_base64',''), validate=True)
                if request is not None and data and len(data)<=MAX_PAYLOAD:
                    self.transport.sendto(data,request[0])
            except (ValueError, TypeError, KeyError) as exc:
                log.warning('invalid manual reply: %s',exc)

async def main_async(port):
    loop=asyncio.get_running_loop()
    protocol=Handoff()
    transport,_=await loop.create_datagram_endpoint(lambda:protocol,local_addr=('127.0.0.1',port))
    print(json.dumps({'event':'ready','udp_listen':f'127.0.0.1:{port}',
                      'mode':'manual_mcp_handoff','automated':False}),flush=True)
    try:await asyncio.gather(protocol.printer(),protocol.reader())
    finally:transport.close()

def main():
    ap=argparse.ArgumentParser()
    ap.add_argument('--udp-port',type=int,default=51820)
    args=ap.parse_args()
    asyncio.run(main_async(args.udp_port))

if __name__=='__main__':main()


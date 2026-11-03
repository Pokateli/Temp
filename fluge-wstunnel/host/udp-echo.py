#!/usr/bin/env python3
"""Temporary loopback-only UDP echo to benchmark an authorized wstunnel relay."""
import argparse
import socket

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--port', type=int, default=51821)
args = parser.parse_args()
if not (1024 <= args.port <= 65535):
    parser.error('port must be in 1024..65535')
with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as sock:
    sock.bind(('127.0.0.1', args.port))
    print(f'UDP echo 127.0.0.1:{args.port}', flush=True)
    while True:
        data, addr = sock.recvfrom(65507)
        sock.sendto(data, addr)

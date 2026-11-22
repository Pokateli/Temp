#!/usr/bin/env python3
"""Measure UDP echo RTT/serial goodput through a wstunnel UDP forwarder.

Run after starting wstunnel server/client:
python3 scripts/benchmark_udp.py --port 15182 --samples 100 --size 1200

This measures serialized round trips, not maximum parallel throughput.
"""
import argparse
import json
import socket
import statistics
import struct
import time


def percentile(values, p):
    values = sorted(values)
    if not values:
        return None
    pos = (len(values) - 1) * p / 100
    lo, hi = int(pos), min(int(pos) + 1, len(values) - 1)
    return values[lo] + (values[hi] - values[lo]) * (pos - lo)


def run(address, port, samples, size, timeout):
    if size < 12 or size > 65000:
        raise ValueError('size must be in [12, 65000]')
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    s.settimeout(timeout)
    s.bind(('127.0.0.1', 0))
    latencies = []
    received = 0
    elapsed_start = time.monotonic()
    for n in range(samples):
        payload = b'FLUG' + struct.pack('!Q', n) + bytes((n % 251,)) * (size - 12)
        started = time.perf_counter()
        s.sendto(payload, (address, port))
        try:
            data, _ = s.recvfrom(65535)
            if data == payload:
                received += 1
                latencies.append((time.perf_counter() - started) * 1000)
        except socket.timeout:
            pass
    elapsed = time.monotonic() - elapsed_start
    s.close()
    return {
        'destination': '%s:%d' % (address, port),
        'samples': samples,
        'payload_bytes': size,
        'received': received,
        'loss_percent': round(100 * (1 - received / samples), 3),
        'p50_ms': round(statistics.median(latencies), 3) if latencies else None,
        'p95_ms': round(percentile(latencies, 95), 3) if latencies else None,
        'max_ms': round(max(latencies), 3) if latencies else None,
        'duration_s': round(elapsed, 3),
        'goodput_mbit_s': round((received * size * 8) / max(elapsed, .0000001) / 1e6, 3),
        'note': 'sequential UDP echo goodput, NOT maximum parallel throughput'
    }


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--address', default='127.0.0.1')
    parser.add_argument('--port', type=int, default=15182)
    parser.add_argument('--samples', type=int, default=100)
    parser.add_argument('--size', type=int, default=1200)
    parser.add_argument('--timeout', type=float, default=1.0)
    args = parser.parse_args()
    if args.samples < 1 or args.timeout <= 0:
        parser.error('samples and timeout must be positive')
    print(json.dumps(run(args.address, args.port, args.samples, args.size, args.timeout), indent=2))

#!/usr/bin/env python3
import asyncio
import sys
from dataclasses import dataclass
from random import randrange
import os


def yield_():
    return asyncio.sleep(randrange(0, 2048) / 1e9)


@dataclass
class Stats:
    total_time: int
    count: int
    last_report: float


async def write_to(sock, data):
    for i in range(len(data)):
        sock.write(data[i:i + 1])
        await sock.drain()


async def send_header():
    reader, writer = await asyncio.open_connection(
        os.environ.get('TARGET_IP', '127.0.0.1'),
        3333,
    )
    HEADER = b'''GET / HTTP/1.1\r
Host: localhost:3333\r
Accept: text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8\r
Accept-Language: en-US,en;q=0.5\r
Accept-Encoding: gzip, deflate\r
Connection: keep-alive\r
Upgrade-Insecure-Requests: 1\r
Priority: u=0, i\r
\r\n'''

    await write_to(writer, HEADER)
    await asyncio.sleep(0.1)
    resp = await reader.read(1000)
    writer.close()
    return resp


async def main(workers):
    await asyncio.gather(*(send_header() for _ in range(workers)))


if __name__ == '__main__':
    asyncio.run(main(int(sys.argv[1])))

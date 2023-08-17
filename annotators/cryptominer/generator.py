#!/usr/bin/env python3

import queue
from threading import Thread
from itertools import product
import re
import socket
import json
import ssl
from datetime import datetime
import shutil
import numpy as np
# dnspython
import dns.resolver

### Config ###
PREFIX = 'blacklists/'
LISTS = [
    'list_cato.txt',
    'list_custom.txt'
]
MAX_THREADS = 20
TMP_OUT = 'verified_miners_tmp.txt'
OUT = 'verified_miners.txt'
### Config ###


def filterWhiteLines(lst):
    return [i for i in lst if i]


def stripList(lst):
    return [i.strip() for i in lst]


def filterComments(lst):
    return [i for i in lst if not i.startswith('#')]


def loadSourceList(filename):
    lines = []
    with open(filename, 'r') as src:
        lines = src.readlines()

    lines = stripList(lines)
    lines = filterWhiteLines(lines)
    lines = filterComments(lines)

    return lines


def dictInitOrAppendSingle(d, k, v):
    if k not in d:
        d[k] = [v]
    else:
        d[k].append(v)


def dictInitOrAppendList(d, k, v):
    if k not in d:
        d[k] = v
    else:
        d[k] += v


def poolsFromList(pools, lst):
    for l in lst:
        # cato list sometimes contains IP address, pass?
        if re.match('^\d+\.\d+\.\d+\.\d+:\d+$', l):
            pass
        elif re.match('^[a-zA-Z0-9\.].*:\d+$', l):
            fqdn, port = l.split(':')
            dictInitOrAppendSingle(pools, fqdn, port)
        elif re.match('^[a-zA-Z0-9\.].*(,\d+)+$', l):
            p = l.split(',')
            dictInitOrAppendList(pools, p[0], p[1:])
    return pools


def getStratumData():
    return '{"id":1,"jsonrpc":"2.0","method":"login","params":{"login":"45pwvVJar9j5eqeQ1L2tQnAp8qHSthzJ1MTuvyW6cMJAbGP9DJBD58DGyLimJsLw5N86yoGkEZyFUQzMaUXmpfCuCX8YLdc"}}\n'


def resolve(fqdn, query, raise_on_empty=False):
    try:
        ips = []
        # StratumV2 endpoints on SlushPool: v2.<location>.slushpool.com/<someValue>
        idx = fqdn.find('/')
        if idx != -1:
            fqdn = fqdn[0:idx]

        answer = dns.resolver.resolve(fqdn, query, raise_on_no_answer=raise_on_empty)
        for rdata in answer:
            ips.append(rdata.address)

        return ips
    except:
        return []


def resolveIp4(fqdn):
    return resolve(fqdn, 'A')


def resolveIp6(fqdn):
    return resolve(fqdn, 'AAAA')


def isStratum(answer):
    try:
        if len(answer) == 0:
            return False

        jsn = json.loads(answer)
        return True
    except:
        return False


def getSocket(ip, port, tls=False):
    ip_type = socket.AF_INET
    addr = (ip, int(port))
    if ip.find(':') != -1:
        ip_type = socket.AF_INET6
        addr = (ip, int(port), 0, 0)

    s = socket.socket(ip_type, socket.SOCK_STREAM)
    s.settimeout(5)

    if tls:
        context = ssl.create_default_context()
        context.check_hostname = False
        context.verify_mode = ssl.CERT_NONE

        s = context.wrap_socket(s, server_side=False)

    s.connect(addr)
    return s


def probeStratumImpl(ip, port, tls):
    try:
        s = getSocket(ip, port, tls=tls)
        s.send(getStratumData().encode())

        answer_bts = b''
        while True:
            c = s.recv(1)
            if c == b'\n' or c == b'':
                break
            answer_bts += c
        s.close()

        answer = answer_bts.decode()
        return answer
    except:
        return ''


def probe(ip, port):
    return probeStratumImpl(ip, port, False)


def probeTls(ip, port):
    return probeStratumImpl(ip, port, True)


def process(pair, q):
    ip, port = pair

    answer = probeTls(ip, port)
    if answer == '':
        answer = probe(ip, port)

    if isStratum(answer):
        q.put('{} {}'.format(ip, port))


def runThreadedProcess(pools, resQueue):
    for pool in pools:
        process(pool, resQueue)


def runThreadedResolve(pools, resQueue):
    for pool, ports in pools:
        ips = []
        ips += resolveIp4(pool)
        ips += resolveIp6(pool)

        for p in list(product(ips, ports)):
            resQueue.put(p)


def getLists():
    global PREFIX
    global LISTS
    return [PREFIX + lst for lst in LISTS]


if __name__ == '__main__':
    miningPoolsDict = dict()
    for src in getLists():
        lst = loadSourceList(src)
        poolsFromList(miningPoolsDict, lst)

    miningPools = np.array([[k, v] for k, v in miningPoolsDict.items()], dtype=object)

    processingStart = datetime.now()

    spliced = np.array_split(miningPools, MAX_THREADS)
    threads = []
    resQueue = queue.Queue()

    for i in range(MAX_THREADS):
        t = Thread(target=runThreadedResolve, args=(spliced[i], resQueue))
        t.start()
        threads.append(t)

    for t in threads:
        t.join()

    # make random permutation of pairs (ip-port)
    # so it is less likely to probe same or similar address in a short time
    resolvedMiningPools = np.random.permutation(list(resQueue.queue))
    poolPairsSpliced = np.array_split(resolvedMiningPools, MAX_THREADS)
    threads = []
    resQueue = queue.Queue()

    for i in range(MAX_THREADS):
        t = Thread(target=runThreadedProcess, args=(poolPairsSpliced[i], resQueue))
        t.start()
        threads.append(t)

    for t in threads:
        t.join()

    processingEnd = datetime.now()
    print('Time: {}'.format(processingEnd - processingStart))

    with open(TMP_OUT, 'w') as dst:
        written = dst.write('\n'.join(list(resQueue.queue)))

    if written > 0:
        shutil.move(TMP_OUT, OUT)

    print('IP Port count: {}, Written: {}'.format(len(resQueue.queue), written))

#!/bin/python3

SPEC_FOLDER = '../specjbb'
JAVA_PATH   = '../zgc-bw/build/linux-x86_64-server-release/jdk/bin/java'
JAVA_XMX    = '16g'
JAVA_XMS    = '16g'

import os
import subprocess
import psutil
import time

CWD = os.getcwd()
NULL = open('/dev/null', 'w')

print("Starting benchmark")

cmd = []
cmd.append(JAVA_PATH)
cmd.append(f'-Xmx{JAVA_XMX}')
cmd.append(f'-Xms{JAVA_XMS}')
cmd.append('-XX:+UseZGC')
cmd.append('-XX:+ZUseBW')
cmd.append(f'-Xlog:gc+zbw=debug:{CWD}/zbw_log.log')
cmd.append(f'-Xlog:gc+zbw+stats:{CWD}/zbw_memory_bw.log')
cmd.append(f'-Xlog:gc+heap:{CWD}/zbw_heap.log')
cmd.append(f'-Xlog:gc:{CWD}/zbw_gc.log')
cmd.append('-jar')
cmd.append('specjbb2015.jar')
cmd.append('-m')
cmd.append('COMPOSITE')
print(' '.join(cmd))

out = open(f'zbw_stdout.log', 'w')
p = subprocess.Popen(cmd, cwd=SPEC_FOLDER, stdout=out, stdin=NULL, stderr=NULL)
p.wait()
out.close()
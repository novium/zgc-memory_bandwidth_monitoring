#!/bin/python3

import re
import math

bw_log     = open('./zbw_memory_bw.log', 'r')
bw_log_out = open('./processed/zbw_memory_bw_processed.log', 'w')

for sample in bw_log.readlines():
    sample = re.sub(r'\[\d*.\d*s\]\[info\]\[gc,zbw,stats\]', '', sample)[1:]
    bw_log_out.write(sample)

bw_log.close()
bw_log_out.close()


gc_log     = open('./zbw_gc.log', 'r')
gc_log_out = open('./processed/zbw_gc_processed.log', 'w')

for line in gc_log.readlines():
    time = re.search(r'\[\d+.\d+s\]\[', line)
    gc   = re.search(r' GC\(\d+\) ', line)

    if time and gc:
        time = math.floor(float(time.group()[1:-3]))
        gc = gc.group()[4:-2]

        gc_log_out.write(str(time) + '\n')
        
gc_log.close()
gc_log_out.close()



# Need to find
# 2s: Searching for high-bound for max-jOPS possible for this system
# 925s: Warmup for building throughput-responsetime curve
# 1105s: Building throughput-responsetime curve
# 7574s: Running for validation
# 7753s: Profiling....|.............................. (rIR:aIR:PR = 2372:2374:2374) (tPR = 35565) [OK]

speclog     = open('./zbw_stdout.log', 'r')
speclog_out = open('./processed/zbw_stdout_processed.log', 'w')

for line in speclog.readlines():
    if ': Searching for high-bound for max-jOPS possible for this system' in line:
        speclog_out.write(re.search(r'\d+s:', line).group()[0:-2] + ' Find_hbIR' + '\n')
    elif ': Warmup for building throughput-responsetime curve' in line:
        speclog_out.write(re.search(r'\d+s:', line).group()[0:-2] + ' Warmup' + '\n')
    elif ': Building throughput-responsetime curve' in line:
        speclog_out.write(re.search(r'\d+s:', line).group()[0:-2] + ' RT-curve_Building' + '\n')
    elif ': Running for validation' in line:
        speclog_out.write(re.search(r'\d+s:', line).group()[0:-2] + ' Validation' + '\n')
    elif ': Profiling' in line:
        speclog_out.write(re.search(r'\d+s:', line).group()[0:-2] + ' Profiling' + '\n')
    elif len(line) > 300:
        speclog_out.write(re.search(r'\d+s:', line).group()[0:-2] + ' slow_step' + '\n')
    elif re.search('[ ]+\d+s: Failed', line) != None or re.search('[ ]+\d+s: Failing', line) != None:
        speclog_out.write(re.search(r'\d+s:', line).group()[0:-2] + ' fail' + '\n')
#!/bin/python3

import matplotlib.pyplot as plt 
import numpy as np
import scipy.ndimage.filters as ndif

def running_mean_uniform_filter1d(x, N):
    return ndif.uniform_filter1d(x, N, mode='constant', origin=-(N//2))[:-(N-1)]

zbw_bw = open('./processed/zbw_memory_bw_processed.log', 'r')

samples = []
for sample in zbw_bw.readlines():
    samples.append(int(sample))

zbw_gc = open('./processed/zbw_gc_processed.log', 'r')
gc_runs = []
for gc in zbw_gc.readlines():
    gc_runs.append(int(gc))

zbw_speclog = open('./processed/zbw_stdout_processed.log', 'r')
zbw_spec = []
zbw_spec_desc = []
for spec in zbw_speclog.readlines():
    zbw_spec.append(int(spec.split(' ')[0]))
    zbw_spec_desc.append(spec.split(' ')[1])

fig, ax = plt.subplots(figsize=(25,7))
plt.scatter(range(len(samples)-1),samples[1:],0.5,label='bandwidth sample')
plt.scatter(gc_runs, [-100000000] * len(gc_runs), 0.1, marker="_", c='g', label='gc')
plt.plot(running_mean_uniform_filter1d(samples[1:], 60), c='r', label='60s mean bandwidth')

trans = ax.get_xaxis_transform()
for i in range(len(zbw_spec)):
    if 'fail' in zbw_spec_desc[i]:
        ax.axvline(x=zbw_spec[i], lw=0.8, linestyle=":", c='red')
        plt.text(zbw_spec[i], 0.99, zbw_spec_desc[i], fontsize=6, rotation=20, transform=trans)
    elif 'slow_step' in zbw_spec_desc[i]:
        ax.axvline(x=zbw_spec[i], lw=1, linestyle="--", c='red')
        plt.text(zbw_spec[i], 0.98, zbw_spec_desc[i], fontsize=8, rotation=20, transform=trans)
    else:
        ax.axvline(x=zbw_spec[i], c='orange')
        plt.text(zbw_spec[i], 0.98, zbw_spec_desc[i], rotation=20, transform=trans)

plt.xlabel('time (s)')
plt.ylabel('memory bandwidth (B/s)')
plt.suptitle('ZGC worker thread memory bandwidth usage in SPECjbb2015')
plt.title(f'1s sample, 16g heap, no large pages, Intel® Xeon® Gold 5218 16c32c @ 2.30GHz, gc-count {len(gc_runs)}, time {len(samples)}s', fontsize=8)
ax.legend()
plt.savefig('test.png',dpi=300)

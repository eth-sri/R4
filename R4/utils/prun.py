#!/usr/bin/env python3

import os
import sys
import concurrent.futures
import subprocess


def run(script, site):
    subprocess.call("%s %s" % (script, site), shell=True)

if __name__ == '__main__':

    if len(sys.argv) < 3:
        print('Usage: %s <script> <proc_max> site_list.txt' % sys.argv[0])
        sys.exit(1)

    with open(sys.argv[3], 'r') as fp:
        sites = fp.readlines()

    with concurrent.futures.ProcessPoolExecutor(int(sys.argv[2])) as executor:
        for site in sites:
            executor.submit(run, sys.argv[1], site)
        executor.shutdown()

    print('Executed %s sites' % len(sites))


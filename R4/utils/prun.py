#!/usr/bin/env python3

import os
import sys
import concurrent.futures
import subprocess


def run(script, site):
    subprocess.call("%s %s" % (script, site), shell=True)

if __name__ == '__main__':

    if len(sys.argv) < 3:
        print('Usage: %s proc_max site_list.txt' % sys.argv[0])
        sys.exit(1)

    with open(sys.argv[2], 'r') as fp:
        sites = fp.readlines()

    run_script = os.path.join(os.path.dirname(os.path.dirname(os.path.realpath(__file__))), 'run.sh')

    with concurrent.futures.ProcessPoolExecutor(int(sys.argv[1])) as executor:

        for site in sites:

            executor.submit(run, run_script, site)

        executor.shutdown()

    print('Executed %s sites' % len(sites))


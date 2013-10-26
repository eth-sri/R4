#!/usr/bin/env python3

import subprocess
import os
import sys


def abs_path(rel_path):
    return os.path.join(
        os.path.dirname(os.path.realpath(__file__)), 
        rel_path
    )

if __name__ == '__main__':

    sites = (
        'http://cs.au.dk/~semadk/',
        'http://google.com',
        'http://eth.ch',
        'http://maps.google.com',
        'http://dir.bg',
        'http://ford.com',
        'http://verizon.com',
        'http://adm.com',
        'http://unitedhealthgroup.com',
        'http://fedex.com'
    )

    if len(sys.argv) > 1:
        sites = sys.argv[1:]

    for site in sites:
        print('Testing %s' % site)
        
        print(' recording...')
        subprocess.check_call([abs_path('clients/Record/bin/record'), "-timeout", "6000", site])
        
        print(' replaying...')
        subprocess.check_call([abs_path('clients/Replay/bin/replay'), site, "/tmp/schedule.data"])

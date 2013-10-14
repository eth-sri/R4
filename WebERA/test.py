#!/usr/bin/env python

import subprocess
import os

def abs_path(rel_path):
    return os.path.join(
        os.path.dirname(os.path.realpath(__file__)), 
        rel_path
    )

if __name__ == '__main__':

    sites = (
	'http://cs.au.dk/~semadk/',
        'http://maps.google.com',
        'http://dir.bg'
    )

    for site in sites:
        print 'Testing %s' % site
        
        print ' recording...'
        subprocess.check_call([abs_path('clients/Record/bin/record'), "-timeout", "6000", site])
        
        print ' replaying...'
        subprocess.check_call([abs_path('clients/Replay/bin/replay'), site, "/tmp/schedule.data"])

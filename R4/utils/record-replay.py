#!/usr/bin/env python3

import subprocess
import os
import sys
import base64

def abs_path(rel_path):
    return os.path.join(
        os.path.dirname(os.path.dirname(os.path.realpath(__file__))), 
        rel_path
    )

if __name__ == '__main__':

    if len(sys.argv) == 1:
        print('Usage: <site file> | <url1> <url2> ...')

    verbose = False
    if '--verbose' in sys.argv:
        verbose = True
        sys.argv.remove('--verbose')

    if len(sys.argv) > 1:
        sites = sys.argv[1:]

    if len(sys.argv) == 2 and os.path.isfile(sys.argv[1]):
        with open(sys.argv[1]) as fp:
            sites = [line.strip() for line in fp]

    results = []

    for site in sites:

        record_log = b''
        replay_log = b''
        success = False

        try:
            print('Testing %s' % site)

            record_cmd = [abs_path('clients/Record/bin/record'), "-autoexplore", "-autoexplore-timeout", "10", "-hidewindow", 'http://%s' % site]
        
            print(' recording... (%s)' % ' '.join(record_cmd))
            record_log = subprocess.check_output(
                record_cmd,
                stderr=subprocess.STDOUT)

            replay_cmd = [abs_path('clients/Replay/bin/replay'), "-timeout", "60", "-hidewindow", 'http://%s' % site, "/tmp/schedule.data"]
        
            print(' replaying... (%s)' % ' '.join(replay_cmd))
            replay_log = subprocess.check_output(
                replay_cmd,
                stderr=subprocess.STDOUT)

            success = True
            
        except subprocess.CalledProcessError as e:
            print(e)
            success = False

        results.append((site, str(success), str(str(record_log).count('Warning') + str(replay_log).count('Warning'))))
        
    print ('')
    print ('=== DONE ===')

    with open('/tmp/result.csv', 'w') as fp:
        for result in results:
            fp.write(','.join(result))
            fp.write('\n')

    print('Result written to /tmp/result.csv')
    

        

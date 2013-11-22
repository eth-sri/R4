#!/usr/bin/env python3

import subprocess
import os
import sys
import base64

def abs_path(rel_path):
    return os.path.join(
        os.path.dirname(os.path.realpath(__file__)), 
        rel_path
    )

if __name__ == '__main__':

    sites = (
	'http://cs.au.dk/~semadk/',
        'http://eth.ch',
        'http://maps.google.com',
        'http://dir.bg',
        'http://ford.com',
        'http://verizon.com',
        'http://adm.com',
        'http://unitedhealthgroup.com',
        'http://fedex.com',
        'google.com',
        'facebook.com',
        'youtube.com',
        'yahoo.com',
        'baidu.com',
        'wikipedia.org',
        'qq.com',
        'linkedin.com',
        'live.com',
        'twitter.com',
        'amazon.com',
        'taobao.com',
        'blogspot.com',
        'google.co.in',
        'sina.com.cn',
        'wordpress.com',
        'yahoo.co.jp',
        'yandex.ru',
        'bing.com',
        'ebay.com',
        'hao123.com',
        'google.de',
        'vk.com',
        '163.com',
        'tumblr.com',
        'pinterest.com',
        'google.co.uk',
        'google.fr',
        'googleusercontent.com',
        'microsoft.com',
        'ask.com',
        'msn.com',
        'google.co.jp',
        'mail.ru',
        'google.com.br',
        'weibo.com',
        'apple.com',
        'paypal.com',
        'tmall.com',
        'google.ru',
        'instagram.com',
        'google.com.hk',
        'xvideos.com',
        'google.it',
        'blogger.com',
        'google.es',
        'imdb.com',
        'sohu.com',
        'craigslist.org',
        '360.cn',
        'soso.com',
        'amazon.co.jp',
        'go.com',
        'xhamster.com',
        'bbc.co.uk',
        'google.com.mx',
        'stackoverflow.com',
        'neobux.com',
        'google.ca',
        'fc2.com',
        'imgur.com',
        'alibaba.com',
        'cnn.com',
        'wordpress.org',
        'adcash.com',
        'flickr.com',
        'espn.go.com',
        'huffingtonpost.com',
        'thepiratebay.sx',
        't.co',
        'odnoklassniki.ru',
        'conduit.com',
        'vube.com',
        'akamaihd.net',
        'gmw.cn',
        'amazon.de',
        'adobe.com',
        'aliexpress.com',
        'reddit.com',
        'google.com.tr',
        'pornhub.com',
        'ebay.de',
        'blogspot.in',
        'about.com',
        'godaddy.com',
        'youku.com',
        'google.com.au',
        'bp.blogspot.com',
        'rakuten.co.jp',
        'google.pl',
        'ku6.com',
        'xinhuanet.com',
        'ebay.co.uk',
        'dailymotion.com',
        'ifeng.com',
        'cnet.com',
        'vimeo.com',
        'uol.com.br',
        'netflix.com',
        'amazon.co.uk'
    )

    if len(sys.argv) > 1:
        sites = sys.argv[1:]

    results = []

    for site in sites:

        record_log = b''
        replay_log = b''
        success = False

        try:
            print('Testing %s' % site)
        
            print(' recording...')
            record_log = subprocess.check_output(
                [abs_path('clients/Record/bin/record'), "-autoexplore", "-pre-autoexplore-timeout", "40",  "-autoexplore-timeout", "40", "-hidewindow", site], 
                stderr=subprocess.STDOUT)
        
            print(' replaying...')
            replay_log = subprocess.check_output(
                [abs_path('clients/Replay/bin/replay'), "-timeout", "180", "-hidewindow", site, "/tmp/schedule.data"],
                stderr=subprocess.STDOUT)

            success = True
            
        except subprocess.CalledProcessError:
            success = False

        results.append((site, str(success), str(str(record_log).count('Warning') + str(replay_log).count('Warning'))))

    print ('')
    print ('=== DONE ===')

    with open('/tmp/result.csv', 'w') as fp:
        for result in results:
            fp.write(','.join(result))
            fp.write('\n')

    print('Result written to /tmp/result.csv')
    

        

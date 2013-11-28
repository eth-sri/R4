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
        'http://google.com',
        'http://facebook.com',
        'http://youtube.com',
        'http://yahoo.com',
        'http://baidu.com',
        'http://wikipedia.org',
        'http://qq.com',
        'http://linkedin.com',
        'http://live.com',
        'http://twitter.com',
        'http://amazon.com',
        'http://taobao.com',
        'http://blogspot.com',
        'http://google.co.in',
        'http://sina.com.cn',
        'http://wordpress.com',
        'http://yahoo.co.jp',
        'http://yandex.ru',
        'http://bing.com',
        'http://ebay.com',
        'http://hao123.com',
        'http://google.de',
        'http://vk.com',
        'http://163.com',
        'http://tumblr.com',
        'http://pinterest.com',
        'http://google.co.uk',
        'http://google.fr',
        'http://googleusercontent.com',
        'http://microsoft.com',
        'http://ask.com',
        'http://msn.com',
        'http://google.co.jp',
        'http://mail.ru',
        'http://google.com.br',
        'http://weibo.com',
        'http://apple.com',
        'http://paypal.com',
        'http://tmall.com',
        'http://google.ru',
        'http://instagram.com',
        'http://google.com.hk',
        'http://xvideos.com',
        'http://google.it',
        'http://blogger.com',
        'http://google.es',
        'http://imdb.com',
        'http://sohu.com',
        'http://craigslist.org',
        'http://360.cn',
        'http://soso.com',
        'http://amazon.co.jp',
        'http://go.com',
        'http://xhamster.com',
        'http://bbc.co.uk',
        'http://google.com.mx',
        'http://stackoverflow.com',
        'http://neobux.com',
        'http://google.ca',
        'http://fc2.com',
        'http://imgur.com',
        'http://alibaba.com',
        'http://cnn.com',
        'http://wordpress.org',
        'http://adcash.com',
        'http://flickr.com',
        'http://espn.go.com',
        'http://huffingtonpost.com',
        'http://thepiratebay.sx',
        'http://t.co',
        'http://odnoklassniki.ru',
        'http://conduit.com',
        'http://vube.com',
        'http://akamaihd.net',
        'http://gmw.cn',
        'http://amazon.de',
        'http://adobe.com',
        'http://aliexpress.com',
        'http://reddit.com',
        'http://google.com.tr',
        'http://pornhub.com',
        'http://ebay.de',
        'http://blogspot.in',
        'http://about.com',
        'http://godaddy.com',
        'http://youku.com',
        'http://google.com.au',
        'http://bp.blogspot.com',
        'http://rakuten.co.jp',
        'http://google.pl',
        'http://ku6.com',
        'http://xinhuanet.com',
        'http://ebay.co.uk',
        'http://dailymotion.com',
        'http://ifeng.com',
        'http://cnet.com',
        'http://vimeo.com',
        'http://uol.com.br',
        'http://netflix.com',
        'http://amazon.co.uk'
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
    

        

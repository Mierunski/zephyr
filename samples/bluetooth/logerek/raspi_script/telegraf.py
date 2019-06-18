#!/usr/bin/env python3
from datetime import datetime
import time
import reelboard
# current date and time
now = datetime.now()

timestamp = time.time()
ret = reelboard.bt_exchange(244)
print('example temp=%f,hum=%d %d' % (ret['T_reel'] / 10, ret['H_reel'], int(time.time() * 1000000000)))
# print("timestamp =", timestamp)
# print("timestamp =%d" % int(timestamp * 1000000000))
# print('example,tag1=a,tag2=b i=4i,j=10i,k=4i %d' % int(time.time() * 1000000000))
# time.sleep(0.1)
# print('example,tag1=a,tag2=b i=5i,j=9i,k=1i %d' % int(time.time() * 1000000000))
# time.sleep(1)
# print('example,tag1=a,tag2=b i=2i,j=20i,k=9i %d' % int(time.time() * 1000000000))
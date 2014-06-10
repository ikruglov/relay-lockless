relay-lockless
==============

udp->tcp lockless relay

relay:
```
result-64-relay:436205
result-128-relay:591715
result-256-relay:603318
result-1024-relay:446428
result-16000-relay:576368
result-32000-relay:583090
result-40000-relay:575539
result-65000-relay:608828
```

relay-lockless:
```
result-64-relay:587371
result-128-relay:587371
result-256-relay:596125
result-1024-relay:562587
result-16000-relay:587371
result-32000-relay:648298
result-40000-relay:579710
result-65000-relay:582241
```

the results are got by:
1. check out https://github.com/demerphq/relay
2. make
3. replace bin/relay by bin/relay from relay-lockless
4. run
```
    $ cd relay/test
    $ ./setup.sh
    $ ./stop.sh
    # after testing is done you can see the results
    $ ./stat.sh | sort -n -t'-' -k 2
```

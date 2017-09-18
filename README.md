# HTTP Server Primitives

## Introduction

This repo demonstrates the attempts to implement a very simple HTTP server using various approaches, and benchmark them using [Apache benchmarking tool (*ab*)](https://httpd.apache.org/docs/2.4/programs/ab.html).

All "request handlers" are forced to sleep for 1 millisecond to simulate the overhead of generating HTTP response; they simply read data from the client socket, send back a fixed length (1K Bytes) of response and close the socket.

The tests are performed on local host, macOS 10.12.6 OS, 2.2 GHz Intel Core i7 (4 cores) processor, 16 GB 1600 MHz DDR3 RAM. Perform benchmarking by following steps:

* `cd` to the directory of the tested server, compile with `g++ -std=c++11 -pthread -o run_server main.cpp`

* start the server by running the executable `./run_server`, it will automatically start running on port 8080

* open another terminal, and execute the shell script under `/benchmark`

```shell
./benchmark/benchmark.sh
```

    * inside the shell script it calls `ab -n {$N} -c {$C} http://127.0.0.1:8080/` where `N` is 10000 for the total number of requests, and `$C` is in `{1 10 100 500}` for concurrent requests
    * the selected output is printed on screen after `ab` finishes; the complete output of `ab` is appended to file `output.log` for reference


## Servers

### 1. Blocking socket, single thread

Implemented with [*POSIX* (Portable Operating System Interface) socket APIs](https://en.wikipedia.org/wiki/Berkeley_sockets#BSD_and_POSIX_sockets). In an infinite loop, server socket `accept()` and blocks if there is no response; while there is a connection, handle it and continue.

Results:

```
command:  ab -n 10000 -c 1 http://127.0.0.1:8080/
Failed requests:        0
Time per request:       1.632 [ms] (mean)
Time per request:       1.632 [ms] (mean, across all concurrent requests)
Requests per second:    612.79 [#/sec] (mean)

command:  ab -n 10000 -c 10 http://127.0.0.1:8080/
Failed requests:        0
Time per request:       13.932 [ms] (mean)
Time per request:       1.393 [ms] (mean, across all concurrent requests)
Requests per second:    717.77 [#/sec] (mean)

command:  ab -n 10000 -c 100 http://127.0.0.1:8080/
Failed requests:        0
Time per request:       137.391 [ms] (mean)
Time per request:       1.374 [ms] (mean, across all concurrent requests)
Requests per second:    727.85 [#/sec] (mean)

command:  ab -n 10000 -c 500 http://127.0.0.1:8080/
Failed requests:        0
Time per request:       690.705 [ms] (mean)
Time per request:       1.381 [ms] (mean, across all concurrent requests)
Requests per second:    723.90 [#/sec] (mean)
```

The server handles concurrent request sequentially thus is slow where concurrency is large. Performance shows CPU usage ~11%.

### 2. Blocking socket, thread-per-connection

Also using POSIX socket that blocks on `accept()`. When there is a connection, start a new thread for that connection and continue; the new thread is detached from the main thread, so the main thread should not touch the resource (socket fd in this case) and the worker thread should close the socket before it terminates.

Results:

```
command:  ab -n 10000 -c 1 http://127.0.0.1:8080/
Failed requests:        0
Time per request:       1.662 [ms] (mean)
Time per request:       1.662 [ms] (mean, across all concurrent requests)
Requests per second:    601.68 [#/sec] (mean)

command:  ab -n 10000 -c 10 http://127.0.0.1:8080/
Failed requests:        0
Time per request:       2.265 [ms] (mean)
Time per request:       0.227 [ms] (mean, across all concurrent requests)
Requests per second:    4414.40 [#/sec] (mean)

command:  ab -n 10000 -c 100 http://127.0.0.1:8080/
Failed requests:        0
Time per request:       5.364 [ms] (mean)
Time per request:       0.054 [ms] (mean, across all concurrent requests)
Requests per second:    18641.55 [#/sec] (mean)

command:  ab -n 10000 -c 500 http://127.0.0.1:8080/
Failed requests:        0
Time per request:       41.297 [ms] (mean)
Time per request:       0.083 [ms] (mean, across all concurrent requests)
Requests per second:    12107.30 [#/sec] (mean)
```

Significantly faster for concurrency, CPU usage (~50%) and CSW are higher due to large amout of context switch.


### 3. Non-blocking socket, thread-per-connection

Using POSIX `select()` to select sockets that are ready for reading.

Results:

```
command:  ab -n 10000 -c 1 http://127.0.0.1:8080/
Failed requests:        0
Time per request:       1.669 [ms] (mean)
Time per request:       1.669 [ms] (mean, across all concurrent requests)
Requests per second:    599.18 [#/sec] (mean)

command:  ab -n 10000 -c 10 http://127.0.0.1:8080/
Failed requests:        0
Time per request:       2.037 [ms] (mean)
Time per request:       0.204 [ms] (mean, across all concurrent requests)
Requests per second:    4908.36 [#/sec] (mean)

command:  ab -n 10000 -c 100 http://127.0.0.1:8080/
Failed requests:        0
Time per request:       11.038 [ms] (mean)
Time per request:       0.110 [ms] (mean, across all concurrent requests)
Requests per second:    9059.35 [#/sec] (mean)

command:  ab -n 10000 -c 500 http://127.0.0.1:8080/
Failed requests:        0
Time per request:       42.331 [ms] (mean)
Time per request:       0.085 [ms] (mean, across all concurrent requests)
Requests per second:    11811.54 [#/sec] (mean)
```

Although socket connecting and I/O are non-blocking, the server is still accepting connection one at a time and start a thread for every connection, leading to similar results as #2.

## Next steps

* thread pooling, keep a pool of threads running to avoid overhead of thread creation/termination and system resource exausting
* use *Boost.Asio* 

## Issues:

* 1. `ab` gets error `apr_socket_recv: Connection reset by peer (54)` when the concurrency level exceeds the system socket connection backlog defined by the `somaxconn` value. On Mac OS X (default: 128), check by `sysctl -a | grep somax` and alter it by `sudo sysctl -w kern.ipc.somaxconn=1024`

* 2. `ab` gets error `apr_pollset_poll: The timeout specified has expired (70007)` after frequent requests; [solution](https://stackoverflow.com/questions/1216267/ab-program-freezes-after-lots-of-requests-why/6699135#6699135)


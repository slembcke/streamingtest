This is just a toy project I made to see what sort of performance one can expect from traditionaly memory mapping + fast lz4 decompression. Microsoft recently announced their DirectStorage API, but without a lot of details. It sounds like the basic idea is to handle multiple IO requests in a single OS call, and also transparently support decompression (at least the XBox will have special hardware for this). The result is advertised to get something like 6 GB/s on the XBox hardware without a lot of CPU cost.

This made me curious what the limits of something simple like memory mapping + lz4 were capable of. The result for 64kb blocks on my i7 ultrabook with an Evo 970+, was between 3 - 5 GB/s running 4 threads. So the bandwidth is there, though certainly at a high CPU cost. Since I could basically load my _entire_ SSD's worth of data in a few seconds I didn't bother benchmarking how much CPU it actually used though. Async IO using epoll or whatever would probably be more efficient than constantly stalling the working threads with page faults, but it was more work, and I was happy enough with the answer I already got. ;)

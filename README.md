# System Programming Lab 12 Multiproccesing vs Multithreading

This program renders Mandelbrot images while supporting multiprocessing (via fork) and multithreading (via pthreads) to reduce runtime.

## Parallel implementation overview (threads)

Each worker process further divides its assigned work into T threads using pthread_create().

- The image is split into contiguous row ranges (sections).
- Each thread receives an arguments struct that contains:
  - pointers to the image buffer and section bounds (start, end)
  - render parameters (xmin, xmax, ymin, ymax, max, etc.)
- Threads compute pixels only inside their own row range, so there is no overlap in writes.
- The process waits on all threads with pthread_join() before finishing its portion of the work.

## How to run

Example (N processes, T threads):

time ./mandel -n 10 -t 20 -x -0.743643887037151 -y 0.131825904205330

## Runtime results table (seconds, real time)

| Nprocs \ Nthreads | 1      | 2     | 5     | 10    | 20    |
|---:|:------:|:-----:|:-----:|:-----:|:-----:|
| 1  | 140.68 | 114.10 | 55.93 | 30.72 | 19.32 |
| 2  | 80.31  | 56.05 | 27.38 | 16.13 | 12.04 |
| 5  | 30.22  | 21.36 | 8.69  | 6.94  | 7.29  |
| 10 | 11.23  | 8.50  | 6.47  | 6.44  | 6.58  |
| 20 | 6.11   | 6.03  | 6.64  | 6.52  | 6.49  |

Best observed runtime: 6.03 s at Nprocs = 20, Nthreads = 2
Baseline runtime: 140.68 s at Nprocs = 1, Nthreads = 1
Approximate speedup: 140.68 / 6.03 ≈ 23.3x

## Discussion

i. Which technique impacted runtime more: multithreading or multiprocessing? Why?

Multiprocessing impacted runtime more.

From the table, increasing Nprocs causes the biggest drops. For example, at 1 thread: 140.68 → 6.11 when going from 1 process to 20 processes. Adding threads helps, but it shows diminishing returns once processes are already high.

Likely reasons:
- Separate processes scale well across CPU cores with less shared-state contention.
- Threads inside a process share memory and can hit limits like memory bandwidth and cache contention.
- Thread creation and scheduling overhead becomes noticeable when the total worker count is large (many processes times many threads).

ii. Was there a “sweet spot” where optimal (minimal) runtime was achieved?

Yes. The best time was at 20 processes and 2 threads (6.03 s).

After that point, adding more threads did not help and sometimes hurt. For example, at 20 processes:
- 2 threads: 6.03 s (best)
- 5 threads: 6.64 s
- 10 threads: 6.52 s
- 20 threads: 6.49 s

This suggests the system hit a hardware or overhead limit, so extra threads increased overhead more than useful parallel work.

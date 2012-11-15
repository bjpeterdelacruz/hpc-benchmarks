# HPC Benchmarks Suite

Type 'make' to update the executables before running each script.

## How to run each script

### block.run.sh

Runs the fileio_block program.

Usage:
```
./fileio_block A B C D
```

A: Smallest block size
B: Largest block size
C: Number of blocks to read from and write to file
D: Number of times that the program will run

### cpu.run.sh

Runs the CPU test in the cpumem program.

Usage:
```
./cpumem A B C D E F
```

A: Number of POSIX threads to use for CPU test
B: Number of times to repeat CPU test
C: Minimize size of array for memory test
D: Maximum size of array for memory test
E: Number of seconds to sleep during memory test
F: Number of times to repeat memory test

Notes:

* Change only A and B for the CPU test.

### io.run.sh

Runs the fileio program.

Usage:
```
./fileio A
```

A: Size of array that will contain characters read in from file

Notes:

* It is recommended that no more than 2 processes per node be used if using very large array sizes.

---

### mem.run.sh

Runs the memory test in the cpumem program.

Usage:
```
./cpumem A B C D E F
```

A: Number of pthreads to use for CPU test
B: Number of times to repeat CPU test
C: Minimize size of array for memory test
D: Maximum size of array for memory test
E: Number of seconds to sleep during memory test
F: Number of times to repeat memory test

Notes:

* Change only C, D, E, and F for the memory test.
* D must be greater than C.
* For memory test, it is recommended that no more than 2 processes per node be used if using very large array sizes.

---

### mm.run.sh

Runs the mm program.

Usage:
```
./mm A B C D
```

A: Number of rows in matrix A
B: Number of columns in matrix A
C: Number of rows in matrix B
D: Number of columns in matrix B

Notes:

* It is recommended that no more than 2 processes per node be used if using very large matrix sizes.

---

### oe.run.sh

Runs the oesort program.

Usage:
```
./oetsort A
```

A: Dimension of square matrix, i.e. number of rows = number of columns

Notes:

* This program runs slow because it uses bubble sort to sort rows and columns of matrix.

---    

### pi.run.sh

Runs the pi program.

Usage:
```
./pi A B
```

A: Number of calculations
B: 1 for Bailey-Borwein-Plouffe algorithm or 2 for Gregory-Leibniz series

---

### prime.run.sh

Runs the prime program.

Usage:
```
./prime A
```

A: Highest number to test for primality

Notes:

* Program is not load-balanced. For example, if A = 500,000,000, process 0 finds 348,513 primes in 7 seconds while process 99 finds 249,760 primes in 67 seconds.

---

### snd.run.sh

Runs the sndrcv program.

Usage:
```
./sndrcv A B
```

A: Size of array that will contain characters
B: Number of times that the program will run

---

### ss.run.sh

Runs the shearsort program.

Usage:
```
./shearsort A
```

A: Dimension of square matrix, i.e. number of rows = number of columns

Notes:

* A must be equal to the number of processes used.
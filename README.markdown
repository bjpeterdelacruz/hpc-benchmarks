# HPC Benchmarks Suite

Type `make` to update the executables before running each script.

## How to run each script

### block.run.sh

Runs the fileio_block program.

Usage:
```
./fileio_block A B C D
```

<table>
<tr><td>A</td><td>Smallest block size</td></tr>
<tr><td>B</td><td>Largest block size</td></tr>
<tr><td>C</td><td>Number of blocks to read from and write to file</td></tr>
<tr><td>D</td><td>Number of times that the program will run</td></tr>
</table>

---

### cpu.run.sh

Runs the CPU test in the cpumem program.

Usage:
```
./cpumem A B C D E F
```

<table>
<tr><td>A</td><td>Number of POSIX threads to use for CPU test</td></tr>
<tr><td>B</td><td>Number of times to repeat CPU test</td></tr>
<tr><td>C</td><td>Minimize size of array for memory test</td></tr>
<tr><td>D</td><td>Maximum size of array for memory test</td></tr>
<tr><td>E</td><td>Number of seconds to sleep during memory test</td></tr>
<tr><td>F</td><td>Number of times to repeat memory test</td></tr>
</table>

Notes:

* Change only A and B for the CPU test.

---

### io.run.sh

Runs the fileio program.

Usage:
```
./fileio A
```
<table>
<tr><td>A</td><td>Size of array that will contain characters read in from file</td></tr>
</table>

Notes:

* It is recommended that no more than 2 processes per node be used if using very large array sizes.

---

### mem.run.sh

Runs the memory test in the cpumem program.

Usage:
```
./cpumem A B C D E F
```

<table>
<tr><td>A</td><td>Number of pthreads to use for CPU test</td></tr>
<tr><td>B</td><td>Number of times to repeat CPU test</td></tr>
<tr><td>C</td><td>Minimize size of array for memory test</td></tr>
<tr><td>D</td><td>Maximum size of array for memory test</td></tr>
<tr><td>E</td><td>Number of seconds to sleep during memory test</td></tr>
<tr><td>F</td><td>Number of times to repeat memory test</td></tr>
</table>

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

<table>
<tr><td>A</td><td>Number of rows in matrix A</td></tr>
<tr><td>B</td><td>Number of columns in matrix A</td></tr>
<tr><td>C</td><td>Number of rows in matrix B</td></tr>
<tr><td>D</td><td>Number of columns in matrix B</td></tr>
</table>

Notes:

* It is recommended that no more than 2 processes per node be used if using very large matrix sizes.

---

### oe.run.sh

Runs the oesort program.

Usage:
```
./oetsort A
```

<table>
<tr><td>A</td><td>Dimension of square matrix, i.e. number of rows = number of columns</td></tr>
</table>

Notes:

* This program runs slow because it uses bubble sort to sort rows and columns of matrix.

---    

### pi.run.sh

Runs the pi program.

Usage:
```
./pi A B
```

<table>
<tr><td>A</td><td>Number of calculations</td></tr>
<tr><td>B</td><td>1 for Bailey-Borwein-Plouffe algorithm or 2 for Gregory-Leibniz series</td></tr>
</table>

---

### prime.run.sh

Runs the prime program.

Usage:
```
./prime A
```

<table>
<tr><td>A</td><td>Highest number to test for primality</td></tr>
</table>

Notes:

* Program is not load-balanced. For example, if A = 500,000,000, process 0 finds 348,513 primes in 7 seconds while process 99 finds 249,760 primes in 67 seconds.

---

### snd.run.sh

Runs the sndrcv program.

Usage:
```
./sndrcv A B
```

<table>
<tr><td>A</td><td>Size of array that will contain characters</td></tr>
<tr><td>B</td><td>Number of times that the program will run</td></tr>
</table>

---

### ss.run.sh

Runs the shearsort program.

Usage:
```
./shearsort A
```

<table>
<tr><td>A</td><td>Dimension of square matrix, i.e. number of rows = number of columns</td></tr>
</table>

Notes:

* A must be equal to the number of processes used.

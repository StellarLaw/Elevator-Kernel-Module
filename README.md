**Project 2: Division of Labor**
**Operating Systems**
```plaintext
Team Members:
1. Alek Coupet
2. Bilal Taleb
3. Sayed Haider

Part 1: System Call Tracing
- All Members
Part 2: Timer Kernel Module
- All Members
Part 3a: Adding System Calls
- All Members
Part 3b: Kernel Compilation
- All Members
Part 3c: Threads
- Sayed
Part 3d: Linked List
- Alek
Part 3e: Mutexes
- Bilal
Part 3f: Scheduling Algorithm
- All Members
```

**Part 1**

**File Listing**

```plaintext
- [empty.c]
- [empty.trace]
- [part1.c]
- [part1.trace]
```
- To Compile and Trace
  ```bash
  gcc -o empty empty.c
  strace -o empty.trace ./empty
  gcc -o part1 part1.c
  strace -o part1.trace ./part1
  ```
**Part 2**

**File Listing**
```plaintext
- [Makefile]
- [src]
    - [my_timer.c]
```
**Compilation**
- While inside part2 directory, to compile the kernel module
  ```bash
  make
  ```
- Load the kernel module
   ```bash
  sudo insmod src/my_timer.ko
   ```
- You can make sure that the module has been loaded
  ```bash
  lsmod | grep my_timer
  ```
- Remove the module
  ```bash
  sudo rmmod my_timer
  ```
- Clean up temporary files
  ```bash
  make clean
  ```
- To use it
  ```bash
  cat /proc/timer
  ```
  - First call will show the current time in seconds from Unix Epoch(1/1/1970) until now
  - Subsequent calls will show the current time and the time elapsed from the previous call
 
**Part 3**

**File Listing**
```plaintext
- [Makefile]
- [src]
  - [elevator.c]
```
**Compilation**

- Navigate to directory where part3 folder is located.
- You should see two things, a Makefile and a folder called src.
- Run the following commands:
```bash
  make
  ```
```bash
  sudo insmod elevator.ko
  ```
```bash
  sudo watch -n1 cat /proc/elevator
  ```
**To Test**

- Navigate to the directory containing the test call program.
  
```bash
  make 
  ```
```bash
  ./run_tests
  ```
- If any errors occur, convert run_test to unix using dos2unix <filename>, and re try.
  
- OR

```bash
  ./producer 8
  ```
```bash
  ./consumer --start
  ```
```bash
  ./consumer --stop
  ```

**To remove**
- When you are finished, navigate back to the part 3 directory.
  
```bash
  sudo rmmod elevator
  ```
```bash
  make clean
  ```

  



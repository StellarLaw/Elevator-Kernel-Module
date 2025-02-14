**Elevator Kernel Module**

**Team Members**
- Alek Coupet
- Bilal Taleb
- Sayed Haidar

**Overview**

This project provides hands-on experience in kernel programming and system-level resource management. It is divided into three parts, each designed to build on the previous:

- System Call Tracing: Adding and verifying system calls in a C program to deepen understanding of their interaction with the kernel.
- Timer Kernel Module: Creating a kernel module to retrieve and display real-time and elapsed time using /proc interfaces.
- Elevator Kernel Module: Developing a kernel module simulating a dorm elevator with realistic constraints, including passenger handling, scheduling, and synchronization using mutexes and kthreads.

It explores essential concepts like concurrency, thread synchronization, and dynamic memory management in a kernel environment. By the end, the completed kernel modules demonstrate effective scheduling, resource management, and robust system programming practices.


**Part 1: System-Call Tracing**

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
**Part 2: Timer Kernel Module**

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
 
**Part 3: Elevator Kernel Module**

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

  



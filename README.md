# Operating System Programming Challenges

This repository showcases a series of programming challenges that focus on system programming, operating systems, and a deeper understanding of advanced concepts such as syscalls and memory management. Below are brief overviews, objectives, and key features for each challenge.

## [Asynchronous Web Server](./async-web-server)

### Objectives

- Deepen knowledge on sockets and asynchronous I/O operations.
- Implement a web server with limited HTTP protocol functionalities using advanced I/O operations.

### Key Features

- Utilizes epoll for multiplexing I/O operations.
- Implements zero-copying with sendfile and non-blocking sockets.
- Serves files from designated directories, distinguishing between static and dynamic content.

## [Memory Allocator](./mem-allocator)

### Objectives

- Implement basic memory management functions.
- Understand and use Linux memory management syscalls.

### Key Features

- A minimalistic memory allocator that includes implementations of `malloc()`, `calloc()`, `realloc()`, and `free()`.
- Utilizes `brk()`, `mmap()`, and `munmap()` for memory allocation and deallocation.

## [Mini-libc](./mini-libc)

### Objectives

- Build a subset of the standard C library to understand low-level syscalls in Linux.
- Gain insights into strings and memory management functions.

### Key Features

- A minimalistic implementation of the standard C library on `x86_64`, focusing on string management, basic memory support, and POSIX file I/O.

## [Minishell](./mini-shell)

### Objectives

- Develop a shell with fundamental functionalities to understand process creation and command execution.
- Explore shell mechanics and error handling.

### Key Features

- Implements a simple Bash-like shell capable of filesystem traversal, running applications, and handling I/O redirection and piping.

## [Parallel Graph](./parallel-graph)

### Objectives

- Explore parallel programming design and implementation.
- Learn to use POSIX threading and synchronization APIs for parallel computation.

### Key Features

- Develops a thread pool to perform graph traversal and compute the sum of node elements, illustrating the efficiency of parallel over serial programs.

This collection of challenges aims to bolster understanding and skills in system programming and operating system concepts, with detailed documentation and implementations available within their respective directories.

## CRDT-Collab
A CRDT based collaborative text editor that allows multiple users at once

## Architecture of CRDT-Collab

```
SyncText/
│
├── include/
│   ├── crdt.h
│   ├── watcher.h
│   ├── broadcaster.h
│   ├── receiver.h
│   ├── operation.h
│   ├── sync_queue.h
│   └── utils.h
│
├── src/
│   ├── main.c
│   ├── crdt.c
│   ├── watcher.c
│   ├── broadcaster.c
│   ├── receiver.c
│   ├── sync_queue.c
│   └── utils.c
│
├── build/
│   └── (compiled .o files)
│
├── Makefile
├── Project.pdf
├── .gitignore
└── README.md
```

# SyncText Collaborative Text Editor

## Project Overview

SyncText is a collaborative text editor that enables multiple users to collaboratively edit a plain text file in real-time over a local network or a single machine. It uses a Conflict-free Replicated Data Type (CRDT) approach to ensure consistent merging of text changes with minimal conflict.

The project is designed to be portable and runs on both Linux and macOS platforms. It employs Unix Domain Sockets for inter-process communication.

---

## How the Code Works

### Components and Architecture

SyncText consists of the following key components spread across different modules/files:

- **Watcher (`watcher.[ch]`)**  
  Monitors the local text file for changes. It polls the file modification time and reads new contents periodically. Line differences are detected and converted into `replace` operations, which are enqueued in a thread-safe queue.

- **CRDT Merge (`crdt.[ch]`)**  
  Maintains an internal document model as an array of lines. It merges incoming operations by applying replace operations on affected lines ensuring last-writer-wins semantics.

- **Operation Queue (`sync_queue.[ch]`)**  
  Implements a lock-free, thread-safe circular queue to communicate operations safely between threads (watcher → broadcaster and receiver → CRDT merge).

- **Broadcaster (`broadcaster_unix.[ch]`)**  
  Runs a dedicated thread that dequeues operations from the queue and sends them to all peer processes using Unix Domain Sockets. Each user has a unique socket path.

- **Receiver (`receiver_unix.[ch]`)**  
  Runs a Unix Domain Socket server listening on a unique socket path for incoming connections from peers. It accepts operation messages, enqueues them, applies CRDT merging, and refreshes the terminal display in real-time.

- **Terminal UI (`utils.[ch]`)**  
  Provides terminal refresh utilities to clear and redraw the text document in the console after each merge.

- **Main Program (`main.c`)**  
  Initializes all components, establishes communication channels, starts threads, and manages the program lifecycle including graceful shutdown on signals.

### Communication Flow

1. The **watcher** detects local file changes and creates `replace` operations.
2. These operations are **enqueued** into the shared `SyncQueue`.
3. The **broadcaster** thread dequeues operations and **sends serialized messages** to all peers’ Unix Domain Socket servers.
4. The **receiver** accepts incoming operation messages, enqueues them in the receive queue.
5. The receiver then **merges** these operations into the local CRDT document representation.
6. Finally, the **UI refresh** function updates the terminal screen showing the latest document state.
7. The cycle repeats, continuously synchronizing edits across all running instances, converging eventually on a consistent document state.

---

## Project Status and Features

- [x] File monitoring via watcher with change detection.
- [x] Thread-safe operation queue for inter-thread messaging.
- [x] Unix Domain Socket based broadcaster and receiver for IPC.
- [x] Basic CRDT merging using line-level replace operations.
- [x] Real-time terminal text refreshing to reflect changes.
- [x] Graceful clean shutdown with signal handling.
- [x] Portable design running on macOS and Linux without `mqueue.h`.

### Future enhancements planned:
- Fine-grained insert and delete operations in CRDT for better concurrency.
- GUI or richer terminal interface.
- More robust conflict resolution.
- Network support beyond local Unix socket based IPC.

---

## Building and Running
```
make

# run as user 1 watching file.txt and connected to user 2 and 3
./sync_text 1 file.txt /tmp/user_2.sock /tmp/user_3.sock 

# run as user 2
./sync_text 2 file.txt /tmp/user_1.sock /tmp/user_3.sock 

# run as user 3
./sync_text 3 file.txt /tmp/user_1.sock /tmp/user_2.sock
```
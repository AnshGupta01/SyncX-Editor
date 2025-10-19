# SyncX Collaborative Text Editor
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
│   ├── registry.h
│   └── utils.h
│
├── src/
│   ├── main.c
│   ├── crdt.c
│   ├── watcher.c
│   ├── broadcaster.c
│   ├── receiver.c
│   ├── registry.c
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



## Project Overview

SyncText is a collaborative text editor that enables multiple users to collaboratively edit a plain text file in real-time over a local network or a single machine. It uses a Conflict-free Replicated Data Type (CRDT) approach to ensure consistent merging of text changes with minimal conflict.

The project is designed to be portable and runs on both Linux and macOS platform with some modification.

### Features

- ✅ Real-time collaborative editing across multiple users  
- ✅ CRDT-based conflict-free merging (Last-Writer-Wins)  
- ✅ Automatic change detection and synchronization  
- ✅ User discovery via shared memory registry  
- ✅ POSIX Message Queue based IPC (macOS & Linux compatible)  
- ✅ Lock-free concurrent programming  
- ✅ Separate local document copies per user  
- ✅ Local document: `local_docs/<user_id>_doc.txt`  

---

### Components and Architecture

SyncText with POSIX Queues consists of the following key components spread across different modules/files:

1. **User Registry (`registry.[ch]`)** – Shared memory based user discovery  
2. **File Watcher (`watcher.[ch]`)** – Monitors local document changes  
3. **CRDT Merge (`crdt.[ch]`)** – Conflict-free document merging  
4. **Broadcaster (`broadcaster.[ch]`)** – Sends operations to peers via POSIX queues  
5. **Receiver (`receiver.[ch]`)** – Receives and merges operations from message queues  
6. **Operation Queue (`sync_queue.[ch]`)** – Thread-safe operation passing  
7. **Terminal UI (`utils.[ch]`)** – Document display and refresh  

---

### Communication Flow

1. The **watcher** detects local file modifications and generates corresponding `replace` or `insert` operations.  
2. These operations are **enqueued** in the shared `SyncQueue`.  
3. The **broadcaster** thread dequeues operations and **sends serialized messages** to all active users’ POSIX message queues.  
4. Each peer’s **receiver** thread listens on its designated message queue and **enqueues incoming operations** into its receive queue.  
5. The **receiver** merges these operations into the local CRDT structure, resolving conflicts automatically.  
6. The **UI layer** refreshes the terminal to reflect the latest synchronized state.  
7. This loop continues seamlessly, ensuring CRDT-based convergence across all user instances.  

---

### Data Flow
```Local Edit → Watcher → Queue → Broadcaster → POSIX MQ → Peer Receiver → CRDT Merge → Display```

### Building and Running Example
```
make clean && make

Terminal 1 (User 1)
./sync_text 1

Terminal 2 (User 2)
./sync_text 2

Terminal 3 (User 3)
./sync_text 3
```
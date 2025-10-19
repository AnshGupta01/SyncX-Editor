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



## Project Overview

SyncText is a collaborative text editor that enables multiple users to collaboratively edit a plain text file in real-time over a local network or a single machine. It uses a Conflict-free Replicated Data Type (CRDT) approach to ensure consistent merging of text changes with minimal conflict.

The project is designed to be portable and runs on both Linux and macOS platforms. It employs Unix Domain Sockets for inter-process communication.

### Features

- ✅ Real-time collaborative editing across multiple users
- ✅ CRDT-based conflict-free merging (Last-Writer-Wins)
- ✅ Automatic change detection and synchronization
- ✅ User discovery via shared memory registry
- ✅ Unix Domain Socket based IPC (macOS & Linux compatible)
- ✅ Lock-free concurrent programming
- ✅ Separate local document copies per user

### Components and Architecture

SyncText consists of the following key components spread across different modules/files:

1. **User Registry (`registry.[ch]`)** - Shared memory based user discovery
2. **File Watcher (`watcher.[ch]`)** - Monitors local document changes
3. **CRDT Merge (`crdt.[ch]`)** - Conflict-free document merging
4. **Broadcaster (`broadcaster.[ch]`)** - Sends operations to peers
5. **Receiver (`receiver.[ch]`)** - Receives and merges operations
6. **Operation Queue (`sync_queue.[ch]`)** - Thread-safe operation passing
7. **Terminal UI (`utils.[ch]`)** - Document display

### Communication Flow

1. The **watcher** detects local file changes and creates `replace` operations.
2. These operations are **enqueued** into the shared `SyncQueue`.
3. The **broadcaster** thread dequeues operations and **sends serialized messages** to all peers’ Unix Domain Socket servers.
4. The **receiver** accepts incoming operation messages, enqueues them in the receive queue.
5. The receiver then **merges** these operations into the local CRDT document representation.
6. Finally, the **UI refresh** function updates the terminal screen showing the latest document state.
7. The cycle repeats, continuously synchronizing edits across all running instances, converging eventually on a consistent document state.

### Data Flow
```Local Edit → Watcher → Queue → Broadcaster → Unix Socket → Peer Receiver → CRDT Merge → Display```

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

#### Each user gets their own:
- Local document: `local_docs/<user_id>_doc.txt`
- Unix socket: `/tmp/user_<user_id>.sock`
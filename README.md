# SyncX Collab

A CRDT-based real-time collaborative text editor using POSIX message queues. Multiple users can edit a shared document simultaneously with automatic conflict resolution.

---

## Features

- Real-time collaborative editing across multiple users
- CRDT Last-Writer-Wins (LWW) conflict resolution
- Automatic change detection and synchronization
- POSIX message queue-based IPC
- Shared memory user registry
- Support for up to 5 concurrent users

---

## Architecture

```
SyncX/
│
├── include/
│   ├── crdt.h
│   ├── watcher.h
│   ├── broadcaster.h
│   ├── receiver.h
│   ├── operation.h
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
│   ├── operation.c
│   └── utils.c
│
├── build/
│   └── (compiled .o files)
│
├── Makefile
├── Project.pdf
├── .gitignore
├── template.txt
├── DESIGN-DOC.md
└── README.md
```

### Components and Architecture

SyncText with POSIX Queues consists of the following key components spread across different modules/files:

1. **User Registry (`registry.[ch]`)** – Shared memory based user discovery  
2. **File Watcher (`watcher.[ch]`)** – Monitors local document changes  
3. **CRDT Merge (`crdt.[ch]`)** – Conflict-free document merging (`crdt_mu` for in-process threads)  
4. **Broadcaster (`broadcaster.[ch]`)** – Sends operations to peers via POSIX queues (function on watcher stack)  
5. **Receiver (`receiver.[ch]`)** – Receives and merges operations from message queues  
6. **Terminal UI (`utils.[ch]`)** – Document display and refresh  

---

### Data Flow
```Local Edit → Watcher → Broadcaster → POSIX MQ → Peer Receiver → CRDT Merge → Display```

---

## Requirements

**Platform:** Linux (Ubuntu 20.04+) 
**Compiler:** GCC 9.4+ with C11 support  
**Libraries:** pthread, rt (POSIX real-time)

---

## Build && Run

```bash
# Clean first then compile
make clean && make

# Fast Testing Method for 5 Users
make test
```

```bash
# Manual Project Running

# Terminal 1
./sync 1

# Terminal 2
./sync 2

# Terminal 3
./sync 3

# Simply press `CTRL + C` and the process will initiate shutdown and cleanup the files
```

Each user gets a local document at `local_docs/user_<id>_doc.txt`.
Edit the file with any text editor - changes sync automatically.

---

## How It Works

1. Watcher thread detects local file modifications
2. Operations generated and broadcast via message queues
3. Receivers on all peers merge operations using CRDT
4. Display updates to show synchronized document state

---

## Known Limitations

- Very large bulk edits (50+ lines) may occasionally miss lines
- Requires sufficient message queue capacity
- Single-machine operation (shared memory-based registry)

---

**Author:** Ansh Gupta (25CS60R34)  
**Course:** CS69201 Computing Lab  
**Institution:** IIT Kharagpur
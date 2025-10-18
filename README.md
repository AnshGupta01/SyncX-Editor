## CRDT-Collab
A CRDT based collaborative text editor that allows upto 5 users at once

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

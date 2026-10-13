---
title: SyncX Editor — Interview Knowledge Base
tags:
  - interview
  - systems
  - posix
  - crdt
  - c11
  - placements
aliases:
  - SyncX KB
  - SyncText interview
cssclasses:
  - interview-kb
---

# SyncX Editor — Project Knowledge Base

> [!info] How to use this note
> This is an interview defense document, not a product spec.
> **Verified** = in the source. **Estimable** = you could measure. **Unverifiable** = do not claim.
> If `DESIGN-DOC.md` disagrees with this file, **trust the code**.

> [!danger] Never say these in an interview
> lock-free pipeline · exponential backoff · real-time · distributed/decentralized *network* editor · character-level CRDT · sequence CRDT / RGA / Yjs (unless saying you did *not* build them) · guaranteed delivery · inotify · 7 production modules

**Code names:** repo `SyncX-Editor`, binary `sync`, docs also say SyncText / SyncX Collab.

**Finalized resume bullets (defend these):**

1. Constructed a C11 multi-process editor for 5 replicas with shm_open/mmap peer discovery and POSIX MQ broadcast, no merge server.
2. Implemented per-line Last-Writer-Wins registers with timestamp and user-ID tie-breaks so concurrent edits converge across 5 replicas.
3. Resolved MQ overflow (`mq_maxmsg=10`) on 40–50 line pastes via non-blocking `mq_send` (5 retries/20ms), reducing dropped ops.
4. Devised two-thread watcher/receiver: 500ms polling, line diffs for I/R/D ops, 10ms `mq_receive` keeping merges off the UI thread.
5. Validated ~6KB Operation payloads (3KB old/new text) against POSIX MQ defaults, keeping depth-10 queues within kernel limits.

---

# Project Overview

## Elevator Pitch (30 Seconds)

I built a **C11 collaborative text editor** where up to **5 processes on one Linux machine** edit the same document. There is **no central merge server**. Processes find each other with **POSIX shared memory** (`shm_open`/`mmap`) and send line-level edits over **POSIX message queues**. Conflicts on the same line are resolved with **Last-Writer-Wins** using timestamps and a user-ID tie-break.

## Technical Pitch (2 Minutes)

Each user runs `./sync <id>`. That process is a **replica**: it owns a local file `local_docs/user_<id>_doc.txt`, an in-memory line array with per-line metadata, and a private POSIX MQ `/queue_user_<id>`.

A **shared-memory registry** (`/synctext_registry`) stores up to 5 `{user_id, queue_name, active}` slots so a replica can discover peer queue names.

A **watcher thread** polls file `mtime` every **500ms**, diffs lines against a snapshot, and emits Insert / Replace / Delete operations. Those ops are **sent immediately** (not queued in-process) with **non-blocking `mq_send`** and **5 × 20ms retries** on `EAGAIN`.

A **receiver thread** polls that replica’s queue every **10ms** and applies ops through **per-line LWW registers**: newer `time()` wins; equal time → **lower `user_id` wins**. The replica then rewrites its file and refreshes a terminal UI.

This is **same-host multi-process replication**, not a networked distributed system. Delivery is **best-effort**. Line **index** is the identity, so this is an **array of LWW-Registers**, not a sequence CRDT. Bulk pastes of 40–50 lines can still drop ops because kernel MQ depth is 10.

## Problem Statement

| Question | Answer |
| --- | --- |
| What problem? | Concurrent editing of one document with automatic merge, no human copy-paste between terminals. |
| Why needed? | CS69201 Computing Lab (IIT Kharagpur): demonstrate IPC, threads, and a consistency rule in C. |
| Who uses it? | Course staff / you, locally on Linux. Not production users. |
| Why valuable (resume)? | Rare campus signal: POSIX shm + MQ + pthreads + a named consistency model, plus a real overflow bug. |
| Why not valuable (honest)? | Single machine, 5 users, polling, data loss on bursts, no tests, no network. |

**Metric tags**

- 5 replicas, 500ms / 10ms, `mq_maxmsg=10`, 5 retries × 20ms, `MAX_TEXT_LEN=3072` → **VERIFIED**
- Drop-rate % on 50-line paste → **ESTIMABLE** (do not invent a number)
- “Real-time”, “distributed”, “zero loss” → **UNVERIFIABLE / false**

---

# Architecture Overview

This is **not** a web app. Map the template sections to what actually exists:

| Template term | SyncX equivalent |
| --- | --- |
| Frontend | ANSI terminal dump in `utils.c` (`\033[H\033[J`) |
| Backend | Each `sync` process (watcher + receiver + CRDT) |
| Database | In-memory `document_lines[]` + `LineMetadata[]`; durable copy is a text file |
| Authentication | **None** |
| Authorization | **None** (queue/shm mode `0666`) |
| Storage | POSIX shm registry; POSIX MQ; `local_docs/*.txt` |
| Deployment | Local Linux binary via `Makefile` |
| Infrastructure | One kernel: `/dev/shm`, `/dev/mqueue` |

## High-Level Architecture Diagram

```
                    POSIX shared memory
                 /synctext_registry (mmap)
                 ┌─────────────────────┐
                 │ UserEntry[5]        │
                 │ id, queue, active   │
                 └──────────┬──────────┘
                            │ peer list (polled ~2s by main)
        ┌───────────────────┼───────────────────┐
        ▼                   ▼                   ▼
   Process 1           Process 2           Process 5
   ./sync 1            ./sync 2            ./sync 5
   ┌─────────────┐     ┌─────────────┐     ┌─────────────┐
   │ main (UI)   │     │ main (UI)   │     │ ...         │
   │ watcher thr │     │ watcher thr │     │             │
   │ receiver    │     │ receiver    │     │             │
   │ CRDT memory │     │ CRDT memory │     │             │
   │ local file  │     │ local file  │     │             │
   └──────┬──────┘     └──────┬──────┘     └─────────────┘
          │ mq_send           │
          └────────► /queue_user_2  (POSIX MQ, depth 10, ~6KB msgs)
                     /queue_user_1
                     /queue_user_N
```

**Threads per process (VERIFIED):**

```
main ── sleep(2) ── refresh peers + UI
watcher ── usleep(500000) ── stat() ── line diff ── on_local_change()
                                                      ├─ mq_send to peers  (broadcaster, NOT a thread)
                                                      ├─ crdt_merge_operation
                                                      └─ crdt_save_to_file
receiver ── usleep(10000) ── mq_receive ── on_remote_operation()
                                              ├─ crdt_merge_operation
                                              └─ crdt_save_to_file
```

> [!warning] Design-doc mismatch
> Docs say a 3-stage watcher–broadcaster–receiver **pipeline** and a lock-free op queue.
> Code: `broadcaster_start()` logs `"no buffer, immediate send"`.

## Request Lifecycle

There is no HTTP request. One **edit lifecycle**:

1. User edits `local_docs/user_N_doc.txt` in vim/nano/VS Code and saves.
2. Watcher `stat()` sees `st_mtime` change (resolution: **1 second** on many filesystems).
3. File is read with `getline`; compared line-by-line to previous snapshot.
4. For each changed index: build `Operation` `{user_id, op_type I/R/D, line_no, col_*, old_text, new_text, time(NULL)}`.
5. `on_local_change`:
   - `broadcaster_add_operation` → for each peer: `mq_open(O_WRONLY|O_NONBLOCK)` → `reliable_mq_send` → `mq_close`
   - apply locally via CRDT
   - rewrite file from CRDT
   - redraw UI
6. Peer receiver: `mq_receive` → `on_remote_operation` → CRDT → rewrite that peer’s file → redraw.

## Data Flow

```
Local Edit
  → Watcher snapshot diff
  → Operation (raw C struct, memcpy as bytes)
  → POSIX MQ (kernel)
  → Peer receiver
  → should_apply_operation (LWW)
  → document_lines[line_no] + line_metadata
  → fwrite all lines to local file
  → printf UI
```

**Persistence:** files are the durable store. Crash mid-write can truncate a doc (**no atomic rename**).

**Discovery plane vs replication plane:**

- Discovery = shm registry (who is alive, queue names)
- Replication = POSIX MQ (the ops)
- Merge = local CRDT (no leader)

---

# Resume Bullet Audit & Defense

## Bullet 1

**Resume bullet:** Constructed a C11 multi-process editor for 5 replicas with shm_open/mmap peer discovery and POSIX MQ broadcast, no merge server.

### Classification

**Strong**, with **Interview Risk** if you say “decentralized / distributed system.”

### What it actually means

Five OS processes share one document. They find each other’s queue names in a **shared memory table**. Edits are **copied into each peer’s message queue**. Nobody runs a merge API. Each replica merges locally.

### Implementation details

- `registry_init`: `shm_open("/synctext_registry", O_CREAT|O_RDWR, 0666)` → `ftruncate(sizeof(UserRegistry))` → `mmap(..., MAP_SHARED)`
- `MAX_USERS = 5`
- `registry_register_user` scans for `active==0` slot; also deactivates stale same `user_id`
- Broadcast: `mq_send` of `sizeof(Operation)` bytes to `/queue_user_<id>`
- C11: `-std=c11` in Makefile

**Gaps (own these):**

- Registry is **not memset** on first create → garbage until slots are written
- **No mutex/semaphore** on registry → concurrent register is racy
- `ftruncate` on every join (same size: usually OK, still sloppy)
- `registry_cleanup` `munmap`s but does **not** `shm_unlink` → leftover object
- “No merge server” ≠ no shared state: **discovery is centralized in shm**

### Why it matters

Engineering: OS primitives, process isolation, IPC. Business: none (lab). Interview: whiteboard of two planes (discovery vs replication).

### Defensibility

**8/10** if you stay on one machine. **3/10** if you say distributed Google Docs.

### Likely questions

**Easy:** What is `shm_open` vs `mmap`? Why 5? What is a replica?

**Medium:** Why MQ not pipes/sockets? What happens if process 1 dies? How does process 3 find process 2?

**Hard:** Is the registry a single point of failure? Memory visibility of `volatile int` across processes? Why not a well-known TCP port?

**Brutal:** You called it decentralized but you have a single shm segment. Defend that. Also: uninitialized shm and register races — is this production IPC?

### Model answers

**Easy:** “A replica is one `./sync` process with its own file, CRDT memory, and MQ. Five is `MAX_USERS`. `shm_open` creates/opens a named shm object; `mmap` maps it into the VA space so all processes see the same bytes.”

**Medium:** “Pipes are 1:1 and anonymous unless you pass FDs. Unix sockets would work and would port to network later; MQ gave kernel-bounded queues and `EAGAIN` backpressure, which matched the lab. If process 1 dies, SIGINT path unregisters; a crash leaves `active=1` until overwritten. Process 3 reads the registry for queue names.”

**Hard:** “Discovery is centralized; merge is not. `volatile` is not an inter-process memory barrier. A robust registry would use a POSIX semaphore or `pthread_mutex` in shm with `PTHREAD_PROCESS_SHARED`, plus `shm_unlink` on last user.”

**Brutal:** “I should not say decentralized-distributed. I should say multi-process, shared-memory discovery, independent LWW merge. The registry is a lab-sized SPOF and is racy. I would not ship it.”

### Knowledge required

`shm_open`, `mmap`, `MAP_SHARED`, `ftruncate`, POSIX MQ names, process vs thread, why 0666 is insecure, difference between no *merge* leader and no *shared* state.

---

## Bullet 2

**Resume bullet:** Implemented per-line Last-Writer-Wins registers with timestamp and user-ID tie-breaks so concurrent edits converge across 5 replicas.

### Classification

**Strong** if you say **per-line LWW-Register**. **Overstated / Interview Risk** if you say **CRDT** without qualification, or “all concurrent edits are kept.”

### What it actually means

Every line index `i` stores `(text, timestamp, user_id)`. When two ops target the **same index**, the later clock wins. If clocks equal, **smaller user_id wins**. The loser is **discarded**, not merged character-wise.

### Implementation details

```c
// should_apply_operation
if (meta->timestamp == 0) apply;
if (op->timestamp > meta->timestamp) apply;
if (op->timestamp == meta->timestamp && op->user_id < meta->user_id) apply;
else reject;
```

- `'I'` and `'R'` both `strdup(op->new_text)` into `document_lines[line_no]`
- `'D'` stores `"\n"` (tombstone-like blank line, not a true CRDT tombstone)
- New indices (`line_no >= line_count`) always apply
- Timestamp = `time(NULL)` → **1 second granularity** → many ties → user_id bias
- Same machine → clocks agree; this **does not** survive NTP-skew hosts
- `col_start` / `col_end` / `old_text` are **not used in merge**

**Not a sequence CRDT:** inserting at line 0 on two replicas does **not** commute. Line numbers shift independently. You can lose a line or mis-align content. Shapiro-style sequence CRDTs use unique IDs (WOOT, RGA, LSEQ, Yjs).

### Why it matters

Shows you know **convergence** vs **intention preservation**. LWW is the simplest CRDT: a register. Interviewers love this if you own the limitation.

### Defensibility

**8/10** with “per-line LWW.” **4/10** with “CRDT so concurrent edits merge.”

### Likely questions

**Easy:** What is LWW? Why user_id tie-break?

**Medium:** LWW vs OT? What is lost on same-line concurrent edit?

**Hard:** Prove replicas converge. Counterexample for inserts. Why not vector clocks?

**Brutal:** Point to the code that makes this a CRDT in the Shapiro sense. (It is a **map of registers**, not a list CRDT.) Is `time()` a causal clock? (No, wall clock.)

### Model answers

**Easy:** “LWW means the write with the greater timestamp is kept. Equal timestamps need a total order; I use lower user_id so every replica picks the same winner.”

**Medium:** “OT transforms ops against each other and needs a central or complex transform; CRDT/LWW is state-based/op-based and commutative for **that register**. Same-line: one user’s characters are overwritten. I did not preserve both intentions.”

**Hard:** “For a given line index, the set of writes is ordered by `(timestamp, -user_id)` so max is unique → all replicas that received the same ops end at the same value for that index. I do **not** have causal broadcast; dropped MQ messages break this. Insert at index 0 is not an op on a unique element ID, so list structure diverges.”

**Brutal:** “I implemented LWW-Registers keyed by line index. I did not implement RGA. Wall-clock LWW is not causal. Given more time I would ID each line with `(user, counter)` and use an ordered list CRDT.”

### Knowledge required

CRDT vs OT, LWW-Register, commutativity, idempotence, tombstones, why wall clocks ≠ Lamport clocks, intention preservation, Yjs/RGA at a conceptual level (not implementation).

---

## Bullet 3

**Resume bullet:** Resolved MQ overflow (`mq_maxmsg=10`) on 40–50 line pastes via non-blocking `mq_send` (5 retries/20ms), reducing dropped ops.

### Classification

**Strongest bullet.** Tiny risk: “Resolved” sounds like **fixed forever**. README still says 50+ lines can miss. Prefer mentally: **mitigated**.

### What it actually means

Pasting 40–50 lines generates ~40–50 messages very fast. Each queue holds **10**. Non-blocking send returns `EAGAIN` when full. You retry 5 times, 20ms apart (max **100ms** extra wait). Some messages still drop.

### Implementation details

```c
#define MAX_SEND_RETRIES 5
#define SEND_RETRY_DELAY_US 20000  // fixed, NOT exponential
// mq_maxmsg = 10
// O_NONBLOCK on send and receive
```

- On `EAGAIN`: sleep 20ms, retry
- Other errno: abort retries
- After 5 failures: log and **drop**
- Each send: `mq_open` + send + `mq_close` (extra syscall cost under burst)

**DESIGN-DOC.md says exponential backoff. Code is constant 20ms. If asked, say the doc is wrong.**

### Why it matters

You hit a **kernel limit**, measured a failure mode, added backpressure. That is senior-shaped debugging for a student project.

### Defensibility

**9/10** if you say best-effort / reduced drops. **6/10** if you say “resolved” = never drops.

### Likely questions

**Easy:** What is `EAGAIN`? Why non-blocking?

**Medium:** Why depth 10? Why not blocking send? At-least-once or at-most-once?

**Hard:** Duplicate delivery? Head-of-line blocking?

**Brutal:** Prove drop rate improved. (You don’t have numbers — say ESTIMABLE.) Why open/close MQ per op?

### Model answers

**Easy:** “`EAGAIN` means the queue is full or would block. Non-blocking keeps the watcher from hanging if a peer is slow.”

**Medium:** “Linux default `fs.mqueue.msg_max` is often 10; requesting more makes `mq_open` fail without raising the sysctl. Blocking send could stall the watcher and miss file edits. Semantics: **at-most-once best-effort**. No sequence numbers, no acks, no retry after the 5 attempts, so not at-least-once.”

**Hard:** “No duplicates from my retry loop if `mq_send` succeeded once. A peer crash after dequeue is a drop. True fix: bigger queue, batching, or credit-based flow control.”

**Brutal:** “I don’t have a measured X% — I would log failed `mq_send` before/after. Opening the queue per message is lazy and slow; I should cache `mqd_t` per peer.”

### Knowledge required

POSIX MQ attrs, `O_NONBLOCK`, `EAGAIN`, sysctl `msg_max` / `msgsize_max`, at-most-once vs at-least-once vs exactly-once, backoff vs retry budget.

---

## Bullet 4

**Resume bullet:** Devised two-thread watcher/receiver: 500ms polling, line diffs for I/R/D ops, 10ms `mq_receive` keeping merges off the UI thread.

### Classification

**Strong** if you do **not** call broadcaster a third worker thread. **Interview Risk** if you say “pipeline” or “real-time.”

### What it actually means

Two pthreads do I/O. Main thread mostly sleeps and reprints the document. Remote merges do not run on main, but they **do** contend with the watcher on CRDT memory **without a mutex**.

### Implementation details

Watcher:

- `stat` every 500ms; trigger on `st_mtime` change
- `st_mtime` is often **second** resolution → two saves in the same second can be missed
- Line `strcmp`; if `i >= old_count` → `I`; `i >= new_count` → `D`; else `R`
- Insert at line 0 looks like **every line replaced** (index-aligned diff, not LCS)
- `find_column_diff` fills `col_start`/`col_end` — merge ignores them
- Callback runs **on the watcher thread** and **blocks on mq_send retries** (up to ~100ms per peer per op)

Receiver:

- `mq_receive` into `Operation` buffer of `sizeof(Operation)`
- Empty queue: `EAGAIN`, sleep 10ms
- Callback on receiver thread: merge + save + UI printf

Main:

- `sleep(2)` then `refresh_peer_list_and_display`
- UI is `printf` from **all three** contexts → interleaved/garbled output possible

**Polling vs inotify:** design chose `stat` for simplicity/portability. `inotify` would be event-driven and avoid 500ms + 1s mtime issues.

### Why it matters

Thread roles, polling vs events, keeping a receive loop off a UI loop (even if UI is just printf).

### Defensibility

**7.5/10**. Drops if you claim lock-free CRDT or three pipeline threads.

### Likely questions

**Easy:** Why two threads? Why 500ms / 10ms?

**Medium:** Why not inotify? Can watcher miss edits? Who writes the CRDT?

**Hard:** Data race details. Could merge of remote op retrigger watcher (write file → mtime change → rebroadcast)?

**Brutal:** You said merges stay off the UI thread — but `utils_display_document` is called from watcher and receiver. Is that “off the UI thread” or “no dedicated UI thread”?

### Model answers

**Easy:** “Watcher produces local ops; receiver consumes remote ops; main refreshes peers/UI every 2s. 500ms was a latency vs CPU tradeoff; 10ms receive poll is a simple non-blocking loop.”

**Medium:** “inotify is better on Linux; I avoided event-type complexity (MODIFY vs CLOSE_WRITE, editors writing via rename). `stat` can miss intra-second saves. Both watcher and receiver call `crdt_merge_operation` — that is a race; the design doc’s single-writer claim is false.”

**Hard:** “Remote merge writes the file. Watcher may see mtime change, diff, and **broadcast those lines as local ops**, amplifying or echoing. I do not tag writes as ‘from CRDT’ vs ‘from user’. Fix: inotify ignore, generation counter, or write via temp+rename and skip watcher with a flag.”

**Brutal:** “Honest rephrase: receive loop is not on main. Display is unsynchronized printf from multiple threads. I used ‘UI thread’ loosely for main’s refresh loop.”

### Knowledge required

pthreads, `stat`/`mtime`, inotify, race conditions, memory models, false sharing not relevant here; file-watch feedback loops.

---

## Bullet 5

**Resume bullet:** Validated ~6KB Operation payloads (3KB old/new text) against POSIX MQ defaults, keeping depth-10 queues within kernel limits.

### Classification

**Weak / Understated constraint.** Technically true. Lowest interview value. Recruiter may not understand it. **Keep only as 5th bullet.**

### What it actually means

Linux often caps MQ messages at **8192 bytes** and **10 messages**. Your `Operation` is roughly **6KB** because `old_text` and `new_text` are 3072 bytes each. You set `mq_msgsize = sizeof(Operation)` and `mq_maxmsg = 10` so `mq_open` succeeds **without sysctl**.

### Implementation details

```c
#define MAX_TEXT_LEN 3072  // comment: increased from 1024
attr.mq_maxmsg = 10;
attr.mq_msgsize = sizeof(Operation);
```

Rough layout (64-bit): two 3072 buffers + ints + `time_t` + padding ≈ **6.1–6.2KB** (`receiver` prints `sizeof(Operation)` at startup — **VERIFIED at runtime**, quote that log if asked).

If `mq_msgsize > msgsize_max` or `mq_maxmsg > msg_max`, `mq_open` fails with `EINVAL`.

### Why it matters

Shows you read kernel limits instead of assuming infinite queues. Weak because it is a **config constraint**, not an algorithm.

### Defensibility

**7/10** as a sizing story. **5/10** as a “validation” achievement (no benchmark harness in repo).

### Likely questions

**Easy:** Why 3KB×2? Why not send just the new line?

**Medium:** How do you know 6KB? What is `msgsize_max`?

**Hard:** Why send the whole struct including unused `old_text`? Padding/ABI? Endianness if networked later?

**Brutal:** Where is the test that `sizeof(Operation) < msgsize_max`? (“I printed sizeof and used kernel defaults” is the honest answer.)

### Model answers

**Easy:** “Each op can carry the previous and next line so a receiver *could* do finer diffs; merge currently ignores old_text. 3072 was raised from 1024 for long lines.”

**Medium:** “I set msg size to the struct size and depth 10 to match common `/proc/sys/fs/mqueue/` defaults. The receiver logs struct size at init.”

**Hard:** “Sending raw structs is not a protocol: padding, alignment, no version field. Fine for same-machine same-binary. Network would need a packed, versioned encoder. Sending old+new doubles size and burns MQ capacity — a real cost during bulk paste.”

### Knowledge required

`struct mq_attr`, sysctls, struct padding, why memcpy of structs is not serialization.

---

# Complete Feature Deep Dive

## F1. Peer discovery (registry)

**Purpose:** Let process N find queue names of others.

**User flow:** Start `./sync N` → see “Registered as user_N”.

**Technical flow:** `shm_open` → `mmap` → scan slots → write `{id, /queue_user_N, active=1}`.

**Edge cases:** Duplicate id (stale slot cleared); 6th user (`No space`); crash leaves active=1.

**Failures:** `shm_open` permission; uninitialized memory looks “active”.

**Security:** `0666` — any local user can join or spoof.

**Scale:** 100 users: shm table must grow; still one host. 1k+: this design dies; need network membership (SWIM, gossip, or a coordinator).

**Improve:** POSIX semaphore, `shm_unlink` on last exit, heartbeat + timeout, `memset` on create (`fstat` size 0).

## F2. File watching and diff

**Purpose:** Turn editor saves into ops without embedding an editor.

**User flow:** Save file → within ~0–500ms (+ mtime second) ops fire.

**Technical flow:** `stat` → read lines → index-aligned `strcmp` → I/R/D.

**Edge cases:** Empty file; missing newline; binary; line longer than 3072 (truncated by `snprintf`); insert shifts all indexes.

**Failures:** `fopen` fail; realloc fail → watcher returns 0 lines (can look like mass delete).

**Security:** Watches a path you created; symlink attacks not considered.

**Scale:** Whole-file read every change is O(n) lines; 1M-line file will hitch.

**Improve:** inotify + LCS or hash-per-line; unique line IDs.

## F3. Broadcast

**Purpose:** Fan-out ops to current peers.

**Flow:** Copy peer list → open each queue → retry send → close.

**Edge cases:** Peer list stale (up to 2s); peer queue unlinked; `peer_count` unsynchronized with main’s updates (**data race** on `peer_queues[]`).

**Failures:** `EAGAIN` after retries; `mq_open` fail.

**Scale:** O(peers) syscalls per op; 5 is fine; 1k peers is a thundering herd.

**Improve:** cache FDs; `mq_notify` or epoll (MQ support varies); batch ops.

## F4. Receive and merge

**Purpose:** Apply remote ops.

**Flow:** non-blocking receive → LWW → save file → UI.

**Edge cases:** `line_no` huge → `ensure_line_capacity` grows with dummy `"\n"` lines (memory DoS from one bad op).

**Failures:** Dropped messages → permanent divergence until overwritten.

**Security:** No signature; anyone who can send to `/queue_user_N` can inject ops.

**Scale:** 10ms poll = 100 wakeups/sec/process even when idle (battery/CPU waste).

## F5. Terminal UI

**Purpose:** Show doc, active users, last change.

**Flow:** Clear screen, print all lines, `fflush`.

**Edge cases:** Multi-thread printf; large docs flood terminal.

**Improve:** mutex around display, or UI only on main via a flag.

## F6. Shutdown

**Purpose:** Ctrl+C cleanup.

**Flow:** `SIGINT` → `running=0` → join threads → `crdt_destroy` → unregister → munmap → delete `local_docs` (**all users’ files** via `cleanup_local_docs`).

> [!warning] `cleanup_local_docs`
> One process exiting **deletes the whole `local_docs/` directory**, including other replicas’ files. Demo-hostile and racy.

---

# Technology Deep Dive

## C11

**What:** ISO C 2011. Used here mainly as GCC `-std=c11`. Concurrency on the live path uses `pthread_mutex` (`crdt_mu`).

**Why:** Course + POSIX + pthreads.

**Alternatives:** C++20, Rust, Go.

**Why not:** Lab constraint; you wanted explicit OS calls.

**Tradeoff:** Manual memory, no standard hashmap, easy races.

**Questions:** What did C11 add (atomics, threads.h — you used pthreads not `thrd_t`)? Difference `_Atomic` vs `volatile`?

## pthreads

**What:** POSIX threads.

**Why:** Concurrent watch and receive.

**Alternatives:** processes only; `poll`/`mq_notify` single-threaded event loop; `libuv`.

**Why not event loop:** More control-flow complexity for a lab.

**Tradeoff:** You introduced races you did not lock.

**Questions:** `pthread_join` vs detach; why `volatile` for `running` (insufficient vs `atomic_bool` or mutex+cond)? Spurious wakeup N/A (you don’t use condvars).

## POSIX shared memory

**What:** Named memory object, file-like, mapped with `mmap`.

**Why:** Fast discovery on one host.

**Alternatives:** A file in `/tmp`; UDP broadcast; Redis; etcd.

**Why not:** Overkill; course wanted shm.

**Tradeoff:** Linux-specific path (`/dev/shm`); not networked; racy without sync.

**Questions:** `MAP_SHARED` vs `MAP_PRIVATE`; why `ftruncate` before mmap; `shm_unlink` lifetime.

## POSIX message queues

**What:** Kernel-backed message list with max count/size, priorities (you use priority 0).

**Why:** Bounded IPC, `EAGAIN` as backpressure.

**Alternatives:** Unix datagram sockets, TCP localhost, pipes, D-Bus.

**Why not sockets:** Would make later networking easier; MQ matched “queue” teaching goal.

**Tradeoff:** Tiny default caps; Linux vs macOS differences (you went Linux-only).

**Questions:** `mq_open` flags; why `mq_unlink` at init (stale queue); priority; `mq_notify`.

## Last-Writer-Wins (as “CRDT”)

**What:** Register whose value is the write with largest timestamp.

**Why:** Simple convergence for **one cell**.

**Alternatives:** OT, RGA, Yjs, central lock, last-save-wins whole file.

**Why not full CRDT:** Time; line granularity accepted.

**Tradeoff:** Loses concurrent same-line intent; index identity breaks lists.

**Questions:** Name three CRDTs; state vs op based; idempotence of LWW apply (applying older op is no-op — good; applying same winner twice OK).

## GCC / Make

**What:** `gcc -Wall -Wextra -O2 -std=c11 -lpthread -lrt`.

**Why:** Standard C toolchain. `-lrt` for MQ/shm on older glibc.

**No Docker, no CI, no sanitizers in Makefile** (`-fsanitize=thread` would scream).

---

# Database Deep Dive

**There is no SQL database.** Three stores:

### 1. CRDT document (primary state)

| Structure | Role |
| --- | --- |
| `char** document_lines` | Text per index |
| `LineMetadata {time_t ts; int user_id}` | LWW metadata |
| `line_count`, `line_capacity` | Grow by `n+20` |

**Relationships:** 1:1 line ↔ metadata.

**Indexes:** none (array is the index).

**Bottlenecks:** O(n) full-file save; O(n) display; insert is not O(1) splice — it overwrites index.

**Scaling:** Unsuitable past modest files; no paging.

### 2. Text files

`local_docs/user_<id>_doc.txt` — non-atomic rewrite. Watcher uses this as both input and output → feedback.

### 3. Registry shm

Fixed array 5. No compaction beyond `active=0`.

### Database interview mapping

If they ask “what’s your schema?”: draw `Operation`, `UserEntry`, `LineMetadata`. Say “I would not use Postgres here; if I networked this, ops log in Kafka/NATS plus a doc store.”

---

# API Deep Dive

**Protocol = binary `Operation` on POSIX MQ.** Not REST.

### Message: `Operation`

| Field | Purpose | Used? |
| --- | --- | --- |
| `user_id` | Replica id, LWW tie-break | Yes |
| `op_type` | `I` `R` `D` | Yes (I and R same apply path) |
| `line_no` | Array index identity | Yes |
| `col_start` `col_end` | Intra-line span | **No** (watcher only) |
| `old_text` `new_text` | 3072 each | new_text yes; old_text unused in merge |
| `timestamp` | `time(NULL)` | Yes |

**Validation:** `line_no < 0` rejected; no max cap; no auth; no schema version.

**Error handling:** Failures are `fprintf` + skip.

**Security:** Spoof `user_id`; giant `line_no`; fill queues.

**Performance:** ~6KB × 50 lines × 4 peers ≈ **1.2MB** burst + 200 `mq_open`s.

**Serialization:** none — send is raw `Operation` struct bytes via `mq_send`.

### Follow-ups

Why I vs R if apply is identical? (Watcher semantics; merge collapsed them.)  
Why no sequence number? (Would detect drops.)  
Endianness? (Same arch only.)

---

# Authentication & Security

**None.** Queue and shm are world-writable `0666`.

### Security weaknesses

- Local privilege: any user on the box injects ops / wipes registry
- No op authentication
- Pathological `line_no` allocation
- `cleanup_local_docs` deletes others’ files
- No bounds on text except `snprintf` truncate
- Signal handler calls `cleanup_local_docs` (not async-signal-safe: `opendir`/`unlink`)

### Improvements

Unix socket with `SO_PEERCRED`; restrict `0600` + shared group; sign ops; don’t free heap in signal handler — set a flag.

### Security interview questions

Is 0666 OK for a lab? (Yes.) For a product? (No.)  
What is TOCTOU on file watch?  
Why signal-safe functions matter?

---

# State Management Deep Dive

**Current:** Process-local heap arrays + mmap’d registry + kernel MQ.

**Why:** Simplest shared-nothing merge with a tiny membership table.

**Alternatives:** One shared mmap of the whole document (locking hell); SQLite; Redis pub/sub.

**Tradeoff:** Each replica can diverge if ops drop; no single source of truth.

**Interview:** “State is per-replica CRDT plus a discovery table. Consistency is eventual **only if** all ops arrive. That assumption is false under MQ overflow.”

---

# Performance Deep Dive

## Existing optimizations

| Item | Reality |
| --- | --- |
| `-O2` | Compiler only |
| Non-blocking MQ | Avoids infinite block |
| Retry | Soft backpressure |
| Poll 500ms | Limits `stat` rate |
| `mq_maxmsg=10` | Fits defaults, **hurts** throughput |

## Bottlenecks

### 1. Kernel queue depth 10

**Problem:** Burst > 10 outstanding msgs.  
**Solution (real):** raise `msg_max`, batch lines, or TCP stream.  
**Complexity:** Low sysctl / medium protocol.  
**Why it works:** Capacity ≥ burst or consumer keeps up.

### 2. Whole-file read/write per change

**Problem:** O(n) per save.  
**Solution:** incremental edits, mmap file.  
**Impact:** Necessary for unique-ID CRDT later.

### 3. Index-aligned diff

**Problem:** Insert at top = N replaces.  
**Solution:** LCS / Myers diff / unique IDs.  
**Impact:** Turns 1 insert into 1 op, not N.

### 4. `mq_open`/`close` per send

**Problem:** Syscall overhead.  
**Solution:** cache descriptors.  
**Impact:** Easy win.

### 5. 10ms busy-ish poll

**Problem:** 100 wakeups/s.  
**Solution:** blocking `mq_receive` on dedicated thread, or `mq_notify`.  
**Impact:** Receiver already a thread — **blocking receive is the obvious fix** and would also drain faster during bursts.

> [!tip] Interview gold
> “I used non-blocking receive + 10ms sleep. A blocking `mq_receive` on the receiver thread would cut idle CPU **and** empty the queue faster, reducing overflow. I should have done that.”

## Potential optimizations (say this)

1. Blocking receive  
2. Cached `mqd_t`  
3. Mutex around CRDT  
4. inotify  
5. Don’t rewrite file on remote merge without suppressing watcher  
6. Line IDs / RGA  
7. `-fsanitize=thread` in debug

---

# Deployment & Infrastructure Deep Dive

| Piece | Status |
| --- | --- |
| Hosting | Your laptop/Linux lab |
| Docker | No |
| CI/CD | No |
| Reverse proxy / DNS / SSL | N/A |
| Monitoring | `printf` |
| Logging | stdout, not structured, not leveled |
| Backups | None |
| `make test` | Starts 5 processes; **no assertions** |

**Interview:** “There is no deployment. Productionizing starts with network transport, auth, tests, and a supervisor (systemd) that doesn’t `rm -rf` peers’ files.”

---

# Production Readiness Review

## Production-ready-ish

- Modular files (`registry`, `watcher`, `crdt`, `broadcaster`, `receiver`)
- Graceful SIGINT path (intent)
- Bounded MQ (doesn’t grow RAM without limit)
- `-Wall -Wextra`

## Missing

Tests, sanitizers, mutexes, heartbeats, durable op log, atomic file write, auth, metrics, packaging, docs matching code

## Operational risks

Orphan shm/mqueues; `make distclean` needed; leftover `/dev/mqueue/queue_user_*`

## Reliability

Best-effort ops; no retransmission log; no anti-entropy (no “sync full doc”)

## Monitoring / logging gaps

No counters: sent, dropped, rejected LWW, queue depth (`mq_getattr`)

## Security / testing / DR gaps

See above. DR: none; files can be half-written.

---

# System Design Expansion

This architecture **does not scale by adding users on one box**. MQ and shm are the wrong substrate past a demo.

## 1,000 users

**Must change:** network (WebSocket/TCP), a membership service, unique line IDs, server-side or P2P gossip. One machine’s `/dev/mqueue` and 6KB×ops will melt.

**DB:** op log (Kafka/NATS) or CRDT doc in Redis/FoundationDB — or a real collab engine (y-websocket).

**Cost:** one VM + managed NATS is enough for a prototype.

## 10,000 users

**Sharding by document**, not by user. Sticky sessions to doc rooms. Fan-out via Redis pub/sub or dedicated collab servers (like Figma’s multiplayer).

## 100,000 users

**Horizontal collab workers**, load balancer by `doc_id`, presence service, compaction of CRDT ops, snapshots.

## 1 million users

**Multi-region**, CRDT persistence, edge WebSocket, OT/CRDT specialists, observability, abuse controls. **Cost:** this is a product company, not a lab Makefile.

### What you would say

“I would throw away POSIX MQ at 1k users. Keep the idea: replicas + op broadcast + merge function. Replace transport and identity of characters/lines.”

---

# Limitations & Ownership

| Component | Limitation | Why (honest) | Interview-safe line | Breaks first |
| --- | --- | --- | --- | --- |
| Registry | Racy, 5 slots, no heartbeat | Lab scope | “Fixed table + shm for class demo; I’d add a mutex and TTL.” | 6th user / crash |
| Watcher | Poll + mtime seconds + index diff | Simplicity vs inotify/LCS | “I chose stat() over inotify to ship faster.” | Rapid saves / insert-at-top |
| CRDT | Line LWW, not list CRDT | Time | “LWW-Register per index, not RGA.” | Concurrent inserts |
| MQ | Depth 10, drops | Kernel default | “I stayed inside msg_max=10 and retried.” | 40–50 line paste |
| Threads | CRDT race | Incorrect single-writer assumption | “I’d put a mutex around merge+save.” | Dual edit+receive |
| Files | Non-atomic save; global cleanup | Demo | “rename() + don’t delete peers’ dirs.” | Crash / Ctrl+C |
| Platform | Linux only | macOS MQ differences | “Scoped to Linux after portability pain.” | macOS build |

## Production improvement plan

**1 day:** Mutex on CRDT; cache `mqd_t`; blocking receive; remove `cleanup_local_docs` from other users; `memset` registry; don’t claim exponential backoff.

**1 week:** inotify; op sequence numbers; log drop counters; TSAN; fix file-write feedback; atomic save.

**1 month:** Line IDs or RGA; tests for concurrent insert; heartbeats; `0600` perms.

**3 months:** TCP transport; multi-host; snapshot + anti-entropy.

**6 months:** Real collab product concerns (auth, persistence, compaction) — or stop; this should not become Google Docs.

## Features intentionally not built

| Feature | Why omitted | Later |
| --- | --- | --- |
| Character CRDT | Too much for lab | RGA/Yjs |
| Network | Course = IPC | TCP + protobuf |
| Undo | Needs history | Op log |
| Auth | Single-user machine assumption | SO_PEERCRED |
| inotify | Complexity | Linux-only watcher |
| Tests | Time | Golden-file convergence tests |

---

# Technical Debt Register

| Area | Current state | Risk | Fix | Priority |
| --- | --- | --- | --- | --- |
| CRDT races | Two writers, no lock | High — UB | Mutex | P0 |
| Design doc lies | Exponential / single-writer / 7 modules | High — trust | Fix doc | P0 |
| MQ drops | Best-effort | High for correctness | Seqnums + resend / full-state sync | P1 |
| Index diffs | Insert = N ops | High | LCS or IDs | P1 |
| Registry sync | No IPC lock | Medium | process-shared mutex | P1 |
| Signal handler | Non-async-safe | Medium | flag only | P2 |
| 0666 perms | Open IPC | Medium (multiuser host) | 0600 | P2 |
| UI from 3 threads | Garbled stdout | Low | mutex / main-only UI | P2 |
| No tests | Regressions | High | make check | P1 |
| memcpy protocol | Not serializable | Low until network | packed struct + version | P3 |
| `time()` LWW | Ties + no causality | Medium | Lamport / hybrid clock | P2 |
| File feedback loop | Echo ops | High | suppress flag | P1 |
| `cleanup_local_docs` | Deletes peers | High in demo | per-user cleanup | P0 |

---

# Tradeoff Analysis

## T1. POSIX MQ vs Unix sockets vs TCP

**Chosen:** POSIX MQ.  
**Why:** Bounded queue, lab IPC, `EAGAIN`.  
**Pros:** Kernel backpressure; named endpoints.  
**Cons:** Size caps; Linux-ish; not multi-host.  
**Long-term:** Must replace for network.  
**Answer:** “MQ taught backpressure. Sockets would have been a better product choice.”

## T2. Line LWW vs RGA vs OT vs file lock

**Chosen:** Per-line LWW.  
**Why:** Implementable in days.  
**Pros:** Easy convergence story for one cell.  
**Cons:** Lost edits; insert identity.  
**Answer:** “I optimized for ship-ability. I know what I would build next (line IDs).”

## T3. Polling vs inotify

**Chosen:** `stat` 500ms.  
**Why:** Simple, fewer event edge cases.  
**Pros:** Easy to explain.  
**Cons:** Latency, mtime granularity, CPU.  
**Answer:** “I would switch to inotify on Linux given another week.”

## T4. Non-blocking + sleep vs blocking receive

**Chosen:** Non-blocking + 10ms.  
**Why:** Uniform with non-blocking send; avoid “stuck” fear.  
**Cons:** Worse drain rate under load (sleep even when queue has messages **after** a read — actually it sleeps **every** iteration including after success, so even a full queue is drained at **≤100 msgs/s**).  

> [!danger] Critical performance fact
> Receiver always `usleep(10000)` even after a successful receive. Max drain ≈ **100 ops/s**. A 50-line paste to a busy peer **cannot** drain faster than that. Combined with depth 10, overflow is structurally likely. **VERIFIED.**

**Answer:** “The 10ms sleep after every receive is a bug-shaped tradeoff. Blocking receive or looping until `EAGAIN` would be better.”

## T5. Same-machine vs networked

**Chosen:** shm + MQ.  
**Why:** Course.  
**Answer:** “Not a distributed system. I will not pretend it is.”

## T6. External editor + file watch vs built-in editor

**Chosen:** Watch a file.  
**Why:** Any editor works; no TUI editor to write.  
**Cons:** mtime, feedback loop, no cursor sync.  
**Answer:** “Files as the UX boundary simplified the project and created the watcher problem.”

---

# Honest Staff Engineer Review

## What impresses you?

C and real POSIX APIs. A named consistency rule. You **noticed** `EAGAIN` on bulk paste. Modular layout. You can talk OS.

## What looks junior?

Design doc / resume drifting from code. Unused “lock-free” queue left in the tree. `volatile` as a concurrency strategy. Sleep-based loops. No tests. Calling this a CRDT product. Signal handler doing libc I/O. Deleting everyone’s files on exit.

## What looks strong?

Overflow mitigation with honest remaining loss. LWW tie-break for deterministic convergence. Clear split discovery vs replication.

## What concerns you?

Correctness: races, dropped ops, index identity, echo from saving. Interview integrity: exponential backoff, lock-free, decentralized.

## What would you change first?

1. Mutex around CRDT+save  
2. Drain MQ without sleeping after every message  
3. Align docs with code  
4. Sequence numbers or full-document resync

## Questions I would ask you

1. Walk me through two users inserting at line 0.  
2. Show me the broadcaster thread. (Trap.)  
3. Is delivery reliable?  
5. How do you know replicas converged?  
6. What’s `sizeof(Operation)` and why does it matter?  
7. `volatile` vs atomics vs mutex for `running` and for shm.

---

# Bluff Detection Report

| Claim | Follow-up | Need to know | Safe answer | Dangerous answer | Revision |
| --- | --- | --- | --- | --- | --- |
| Decentralized | CAP, partitions | It’s one kernel | “No merge leader; discovery is shm.” | “It’s a distributed system.” | multi-process |
| CRDT | RGA/Yjs | Register vs sequence | “LWW-Register per line index.” | “Conflict-free for all edits.” | per-line LWW |
| Pipeline threads | Show broadcaster thread | Not a thread | “Send is inline from watcher.” | “Three worker threads.” | two-thread |
| Resolved overflow | Never drops? | README 50+ | “Reduced EAGAIN drops; still best-effort.” | “Never lose ops.” | mitigated |
| Real-time | Latency SLO | 500ms+1s mtime | “Sub-second typical, not RT.” | “Real-time system.” | drop word |
| Lock-free | Memory orders | Queue unused; CRDT racy | “I wrote an unused SPSC ring.” | “The editor is lock-free.” | delete claim |
| Exponential backoff | Code | 20ms fixed | “Doc is wrong; constant delay.” | “Exponential.” | fixed 20ms |
| 7 modules simplify debugging | How? | Module count | “I split files by concern.” | “Seven production services.” | soften |
| Off the UI thread | Who prints? | All threads print | “Receive isn’t on main; UI isn’t isolated.” | “Classic UI-thread architecture.” | careful wording |
| Validated 6KB | Test harness? | sizeof log | “Sized against default 8KB/10.” | “I benchmarked kernel internals.” | sized, not validated-at-scale |

---

# Cross-Question Simulator

Short **ideal answers**. 100 questions.

## Level 1 — Recruiter (1–12)

1. **What is SyncX?** Collaborative editor, 5 processes, one Linux machine, C11.  
2. **What did you use?** C, pthreads, POSIX shm, POSIX MQ.  
3. **How many users?** 5.  
4. **Is it an app people use?** No, a lab systems project.  
5. **Frontend?** Terminal printout.  
6. **Cloud?** No.  
7. **Your role?** Sole author.  
8. **Hardest bug?** MQ overflow on bulk paste.  
9. **Biggest win?** LWW + IPC without a merge server.  
10. **Production?** No.  
11. **Why C?** Course + OS control.  
12. **CRDT in one sentence?** Each line keeps the newest write.

## Level 2 — Project discussion (13–28)

13. **Why no server?** Lab goal: replicas merge locally.  
14. **How do peers meet?** mmap registry.  
15. **What is an operation?** Struct of line change.  
16. **How fast is sync?** Poll 500ms plus queue; not instant.  
17. **What if two people type the same line?** One wins (LWW).  
18. **What if they type different lines?** Both apply if indexes don’t collide.  
19. **Linux only?** Yes; MQ limits differed on macOS.  
20. **How do you run it?** `./sync 1` … `./sync 5`.  
21. **Tests?** `make test` launches processes; no asserts.  
22. **What would you add first?** Mutex + blocking receive + tests.  
23. **Why files not ncurses editor?** Reuse real editors.  
24. **Memory leaks?** Free on destroy; watcher reallocs snapshots.  
25. **Max line length?** 3072.  
26. **Max lines?** Heap until OOM; bad `line_no` grows array.  
27. **Does Ctrl+C clean MQ?** `mq_unlink` on stop; init also unlinks.  
28. **Shared memory name?** `/synctext_registry`.

## Level 3 — Implementation (29–50)

29. **Show thread creation.** `pthread_create` in `watcher_start` / `receiver_start`.  
30. **Broadcaster thread?** Does not exist.  
31. **Retry constants?** 5 × 20ms.  
32. **Why `mq_unlink` before `mq_open`?** Drop stale queue.  
33. **I vs R in merge?** Same overwrite path.  
34. **Delete representation?** `"\n"`.  
35. **Tie-break?** Lower `user_id`.  
36. **Clock?** `time(NULL)`.  
37. **Column diff used?** No.  
39. **Serialize used?** No.  
40. **`volatile running` enough?** Not a full barrier; works in practice on x86, not correct C.  
41. **Registry lock?** None.  
42. **Peer list race?** Main writes `peer_queues`, broadcaster reads.  
43. **File save atomic?** No.  
44. **getline?** POSIX, dynamic line buffer in watcher.  
45. **Why `strdup`?** Own heap copy of line text.  
46. **`ensure_line_capacity`?** realloc + pad `"\n"`.  
48. **Display ANSI?** `\033[H\033[J`.  
49. **`MAX_USERS` where?** `registry.h`.  
50. **Makefile libs?** `-lpthread -lrt`.

## Level 4 — Architecture (51–68)

51. **Discovery vs replication?** shm vs MQ.  
52. **SPOF?** Registry; also each replica’s correctness.  
53. **Consistency model?** Eventual **if** no drops; otherwise divergence.  
54. **Partition?** Can’t partition one host except pause a process.  
55. **Leader election?** None.  
56. **Idempotent apply?** Older LWW ops no-op; duplicates of winner OK.  
57. **Causal order?** Not guaranteed; MQ is per-peer FIFO but multi-sender interleaving + drops.  
58. **Backpressure?** `EAGAIN` + retry then drop.  
59. **Flow control credit?** No.  
60. **Why depth 10?** Kernel default.  
61. **Fan-out complexity?** O(peers) per op.  
62. **Could you mmap the document?** Yes, with heavy locking; I didn’t.  
63. **Why not Redis pub/sub?** Out of scope / not C-lab.  
64. **Module boundaries?** OK; dead queue module.  
65. **Error model?** Log and continue.  
66. **Observability?** printf.  
67. **Config?** Compile-time `#define`s.  
68. **Portability?** Linux.

## Level 5 — System design (69–82)

69. **10 users?** Grow table, still one host, MQ worse.  
70. **Multi-host?** TCP, real clocks or Lamport, auth.  
71. **1k docs?** Shard by doc id.  
72. **Presence?** Heartbeats, not shm flags.  
73. **Persistence?** Snapshot + op log.  
74. **Compaction?** CRDT GC / snapshots.  
75. **Mobile clients?** This architecture no; need servers.  
76. **Conflict UX?** Show both lines / color by user — not built.  
77. **Offline edit?** Op log and replay; not built.  
78. **Security at scale?** Authn/z, rate limit, size limit.  
79. **Cost?** Lab: $0. Product: collab servers dominate.  
80. **Compare Figma/Google Docs?** They use OT or CRDT + huge infra; I built a teaching replica.  
81. **Why not Yjs?** Wrong language/scope; I’d use it for a web product.  
82. **Anti-entropy?** Periodic full-hash compare and resend snapshot.

## Level 6 — Senior follow-ups (83–92)

83. **Memory model of shm `volatile int`?** Insufficient; need atomics with seq-cst or mutex.  
84. **TSAN result?** Would flag CRDT and peer list.  
86. **Priority inversion?** Not using RT priorities.  
87. **`time()` ties under concurrent saves?** Very likely; user_id bias means user 1 always beats user 5 in the same second.  
88. **Fairness?** LWW+min id is unfair. Hybrid logical clocks still need a tie-break.  
89. **Proof of convergence?** Only for register + reliable broadcast of all ops. You don’t have reliable broadcast.  
90. **Signal safety?** UB risk.  
91. **TOCTOU file watch?** Editor rename vs in-place write.  
92. **Would you hire yourself on this?** For intern systems: yes if honest. For distributed team: only if I don’t oversell.

## Level 7 — Bluff detection (93–100)

93. **Point to the broadcaster pthread.** There isn’t one.  
94. **Show exponential backoff math.** There isn’t; 20ms constant.  
96. **Show inotify.** None.  
97. **Show mutex.** None.  
98. **Show test asserting convergence.** None.  
99. **Show network code.** None.  
100. **Is this a CRDT document type from the literature?** No. It is LWW registers in an array.

---

# Cheat Sheet (15-Minute Revision)

## Elevator pitch

5 Linux processes, C11, shm discovery, MQ ops, per-line LWW, no merge server, not a network app.

## Architecture

```
file save → watcher 500ms → Operation → mq_send retry → peer mq → receiver 10ms → LWW → file
membership: mmap UserEntry[5]
threads: main + watcher + receiver (NOT broadcaster)
```

## Database

Heap lines + metadata; text files; shm table. No SQL.

## APIs

Binary `Operation` ~6KB on POSIX MQ. No HTTP.

## Authentication

None. `0666`.

## Security

Local IPC spoofing; giant line_no; unsafe signal handler.

## Performance

Drain ≤100 ops/s (10ms sleep). Queue depth 10. Overflow on 40–50 line paste. Index insert = N replaces.

## Deployment

`make && ./sync N`. No Docker/CI.

## Key tradeoffs

MQ vs sockets · LWW vs RGA · poll vs inotify · drop vs block

## Key limitations

Drops, races, line-index identity, one host, no tests, docs≠code

## Scaling plan

Throw away MQ at multi-host; keep “ops + merge”; add IDs, TCP, snapshots.

## Top 30 interview answers

1. **Not distributed** — one kernel, multi-process.  
2. **No merge server** — each replica applies LWW.  
3. **Discovery is shm** — that part is centralized.  
4. **5 users** — `MAX_USERS`.  
5. **Two threads + main** — broadcaster is a function.  
6. **500ms / 10ms** — `usleep` values.  
7. **LWW** — max (time, then min user_id).  
8. **Same-line conflict** — one discarded.  
9. **Insert conflict** — indexes don’t commute.  
10. **Not RGA/Yjs.**  
11. **`time()`** — wall clock, 1s ties.  
12. **MQ depth 10** — kernel default.  
13. **~6KB msgs** — 3KB old + 3KB new.  
14. **EAGAIN** — full non-blocking queue.  
15. **5×20ms** — **not** exponential.  
16. **Still drops** — best-effort.  
17. **At-most-once.**  
19. **Column diff unused.**  
20. **I and R merge the same.**  
21. **No mutex on CRDT** — real race.  
22. **File write can retrigger watcher.**  
23. **inotify not used.**  
24. **Linux-only.**  
25. **0666** — lab only.  
26. **Ctrl+C may delete all local_docs.**  
27. **I’d add mutex, blocking receive, seqnums.**  
28. **Scale-out = new transport.**  
29. **Design doc is wrong in places — I go with code.**  
30. **Strongest story: overflow debugging.**

---

# Final Assessment

| Axis | Score |
| --- | --- |
| Resume strength | 7.5/10 |
| Resume defensibility | 8/10 (if you use the safe language in this note) |
| Technical depth | 8/10 (OS) / 5/10 (if they expect web) |
| System design depth | 6/10 (you must *talk* the rewrite, not claim you built it) |
| Production readiness | 4/10 |
| Placement readiness | 8/10 for systems/OS internships |
| Interview readiness | 8.5/10 after memorizing this file |

1. **Impresses most:** POSIX fluency + LWW + overflow story, owned limitations.  
2. **Concerns most:** Overclaiming CRDT/distributed; code vs design doc.  
3. **First deep question:** “Walk me through a concurrent edit. What is a replica? Where is the server?”  
4. **Strongest bullet:** #3 overflow / retry.  
5. **Riskiest bullet:** #2 if you say CRDT loosely; #1 if you say decentralized; #5 if you can’t explain kernel MQ limits.  
6. **Close before interviews:** CRDT vs OT vs RGA; POSIX MQ sysctls; `volatile` vs mutex; at-most-once; why line index isn’t an identity; TSAN/races; blocking vs non-blocking receive drain math (100 ops/s).  
7. **I would probe hardest:** “Show me the third thread. Show me exponential backoff. Two users insert at line 0. Does the watcher rebroadcast remote merges?”

---

## Appendix A — File map

| File | Role | On resume? |
| --- | --- | --- |
| `src/main.c` | Wiring, signals, 2s peer refresh | Yes (lifecycle) |
| `src/registry.c` | shm discovery | Yes |
| `src/watcher.c` | poll + diff | Yes |
| `src/broadcaster.c` | mq_send + retry | Yes |
| `src/receiver.c` | mq_receive loop | Yes |
| `src/crdt.c` | LWW apply | Yes |
| `include/operation.h` | `Operation` wire struct | Yes |
| `src/utils.c` | UI + dangerous cleanup | Careful |
| `Makefile` | `-O2 -std=c11 -lrt` | Passing mention |

## Appendix B — Constants cheat

| Name | Value | File |
| --- | --- | --- |
| `MAX_USERS` | 5 | registry.h |
| `REGISTRY_NAME` | `/synctext_registry` | registry.h |
| Queue name | `/queue_user_%d` | main/receiver |
| `MAX_TEXT_LEN` | 3072 | operation.h |
| `mq_maxmsg` | 10 | receiver.c |
| `MAX_SEND_RETRIES` | 5 | broadcaster.c |
| `SEND_RETRY_DELAY_US` | 20000 | broadcaster.c |
| Watcher period | 500000 µs | watcher.c |
| Receiver period | 10000 µs | receiver.c |
| Main refresh | 2 s | main.c |
| `SYNC_QUEUE_SIZE` | 1024 | unused |

## Appendix C — Metric tags

| Claim | Tag |
| --- | --- |
| 5 replicas, intervals, retries, depth 10, 3KB fields | VERIFIED |
| Overflow on 40–50 line paste | ESTIMABLE (documented observation; not a unit test) |
| Drop % reduced | ESTIMABLE (measure first) |
| sizeof ~6KB | VERIFIED at runtime via printf; ESTIMABLE from struct |
| Distributed, real-time, zero loss, lock-free editor | UNVERIFIABLE / false |

---

*End of knowledge base. If you only have 15 minutes, read the Cheat Sheet and Appendix C. If you have one hour, drill Bluff Detection and questions 83–100.*

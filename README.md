# Robust TFTP Client-Server Implementation (RFC 1350)

A crash-proof implementation of the **Trivial File Transfer Protocol (TFTP)** written in C using UDP sockets. The project pairs a resilient server with an interactive client shell engineered to survive packet loss, ghost packets, and state desynchronization.

## Features

- **Complete TFTP Protocol (RFC 1350):** Supports Read Requests (`get` / RRQ) and Write Requests (`put` / WRQ) using standard 512-byte data blocks and `< 512`-byte end-of-file signaling.
- **Interactive Client Shell:** `tftp>` CLI supporting `connect`, `get`, `put`, `mode`, `help`, and `quit`/`bye`.
- **Transfer Modes:** Dynamic switching between `octet` (binary) and `netascii` (text).
- **Network Reliability & Timeout Recovery:**
  - Uses 1-second `SO_RCVTIMEO` socket timeouts for automatic packet retransmission.
  - Strict state machines ignore duplicate packets, network noise, and residual ghost packets.
  - Enforces `socklen_t` memory boundaries before every `recvfrom()`/`sendto()` to prevent UDP value-result address truncation.
- **RFC-Compliant Error Handling:**
  - **Error 1 (`File not found`):** Rejects download requests for missing files and removes empty local files.
  - **Error 2 (`Access violation`):** Handles file permission failures gracefully.
  - **Error 4 (`Illegal TFTP operation`):** Rejects unrecognized or corrupted opcodes.
  - **Error 6 (`File already exists`):** Prevents server-side overwrites during uploads.
- **Memory & Crash Safety:** Hardened against negative `recvfrom()` integer underflows, malformed headers (`< 4` bytes), double-free file closures, and abrupt client disconnects (including the Final ACK edge case).
- **Sandboxed Directories:** Client and server run in isolated directories (`client_dir/` and `server_dir/`) to safely simulate remote transfers locally.

## Project Structure

```text
.
├── Makefile          # Build automation with explicit source/header dependencies
├── tftp.h            # TFTP macros, opcodes, error codes, and packet prototypes
├── tftp.c            # RFC 1350 packet serialization (RRQ, WRQ, DATA, ACK, ERROR)
├── udp.h             # UDP socket helper prototypes
├── udp.c             # IPv4 UDP socket creation and port binding
├── client.c          # Interactive TFTP client implementation
├── server.c          # TFTP server implementation
├── client_dir/       # Sandboxed directory for client execution and downloads
└── server_dir/       # Sandboxed directory for server execution and hosted files
```

## Building the Project

From the project root, compile both binaries with:

```bash
make
```

This builds `client` and `server` and places them into `client_dir/` and `server_dir/`.

To remove compiled binaries and object files:

```bash
make clean
```

## Usage

### 1. Start the Server

```bash
cd server_dir
./server 3333
```

The server binds to port `3333` and listens indefinitely for incoming requests.

### 2. Start the Client

```bash
cd client_dir
./client
```

You can also connect immediately on startup by passing the server IP:

```bash
./client 127.0.0.1
```

### 3. Supported Client Commands

| Command | Description |
| :--- | :--- |
| `connect <server-ip>` | Set the target TFTP server IP address |
| `get <file-name>` | Download a file from the server |
| `put <file-name>` | Upload a local file to the server |
| `mode <octet \| netascii>` | Set the transfer mode (default: `octet`) |
| `help` | Display all supported commands |
| `quit` / `bye` | Exit the client application |

## Testing Edge Cases

1. **Error 1 (File Not Found):** Run `get fakefile.txt`. The server replies with Error Code 1, and the client prints `Server Error 1: File not found` and deletes the temporary empty file.
2. **Error 6 (File Already Exists):** Run `put f1.txt` when `f1.txt` already exists in `server_dir/`. The server replies with Error Code 6, and the client aborts cleanly without overwriting the file.
3. **Packet Loss & Timeouts:** Both peers track expected block numbers and automatically retransmit their last `DATA` or `ACK` packet on timeout until recovery or `TIMEOUT_LIMIT`.

## Key Engineering Challenges Resolved

| Issue | Root Cause | Resolution |
| :--- | :--- | :--- |
| **1-Second Delay per Block** | `recvfrom()` modifies `socklen_t` as a value-result argument; reusing the shrunken length in `sendto()` truncated the port/address. | Reset `server_len = sizeof(server_addr)` before every `recvfrom()` and passed `sizeof(server_addr)` explicitly to `sendto()`. |
| **Server Segmentation Fault** | On disconnect, `recvfrom()` returned `-1`; `bytes_received - 4` yielded `-5`, which wrapped to a huge unsigned `size_t` in `fwrite()`. | Added strict `bytes_received < 0` error handling and `bytes_received < 4` header validation before any file writes. |
| **Ghost Packet Confusion** | Stale UDP packets from prior sessions reset `timeout_count = 0` prematurely, breaking timeout recovery. | Moved `timeout_count = 0` strictly inside the exact expected-block match branch. |
| **Protocol Desync Deadlock** | Silently ignoring duplicate older blocks left the peer stuck waiting for an ACK. | Retransmitted the last `ACK`/`DATA` packet whenever an older block number is received. |
| **Final ACK Problem** | The client exits immediately after sending the final ACK; if that ACK is dropped, the server times out. | Added a final-block check in the server's timeout handler to treat a timeout after a `< 512`-byte block as a completed transfer. |
| **Double-Free Crash** | Closing `fp` on error and again at the end of the loop caused a double `fclose()`. | Guarded file cleanup with `if (fp != NULL)` and unified exit paths. |
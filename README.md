# Multi-Client TCP Chat Application using C Socket Programming

A multi-client command-line chat application developed in **C** using **TCP socket programming** and **POSIX threads**.

The application uses a centralized **client-server architecture**, where multiple clients connect to a TCP server and communicate through it.

## Architecture

```text
                         ┌──────────────────────┐
                         │      TCP Server      │
                         │      Port: 8080      │
                         └──────────┬───────────┘
                                    │
                    ┌───────────────┼───────────────┐
                    │               │               │
                    ▼               ▼               ▼
                Client A         Client B         Client C
```

Each client establishes a TCP connection with the server. The server maintains connected clients and handles message delivery between them.

## Features

* TCP socket communication
* Multi-client support
* POSIX thread-based client handling
* Username registration
* Duplicate username detection
* Broadcast messaging
* Private messaging
* Online user listing
* Graceful client disconnection
* Client reconnection
* TCP message framing using newline delimiters
* LAN-based communication
* Wireshark packet analysis
* IPv4 TCP communication

## Chat Commands

### `/users`

Displays currently connected users.

```text
/users

Online users:
1. Rahul
2. Jagan
```

### `/msg`

Sends a private message to a specific connected user.

```text
/msg Jagan Hello Jagan
```

Example output:

```text
[Private] Rahul: Hello Jagan
```

### `/quit`

Gracefully disconnects the client from the server.

```text
/quit
```

## Technologies Used

* C
* Linux / WSL
* POSIX Socket API
* TCP/IP
* IPv4
* POSIX Threads (`pthread`)
* Wireshark
* GCC
* Git / GitHub

## Project Structure

```text
multi-client-tcp-chat/
├── src/
│   ├── client.c
│   └── server.c
├── docs/
├── screenshots/
├── README.md
└── .gitignore
```

Development and backup source files are maintained locally and excluded from the Git repository using `.gitignore`.

## Compilation

Navigate to the source directory:

```bash
cd src
```

Compile the server:

```bash
gcc -Wall -Wextra -pthread server.c -o server
```

Compile the client:

```bash
gcc -Wall -Wextra -pthread client.c -o client
```

## Running the Application

### Start the Server

On the server machine:

```bash
./server
```

The server listens on TCP port `8080`.

### Start a Client

From a client machine:

```bash
./client <SERVER_IP> 8080
```

Example:

```bash
./client 192.168.0.114 8080
```

The client then prompts for a username and establishes a TCP connection with the server.

## LAN Testing

The application was tested across two systems connected through the same local network.

Example test topology:

```text
Client Machine
192.168.0.106
       │
       │ TCP :8080
       ▼
Server Machine
192.168.0.114
```

The LAN test successfully established TCP connections and exchanged chat messages between separate machines.

## TCP Message Framing

TCP provides a **byte stream** rather than preserving application-level message boundaries.

To handle message boundaries, the application uses **newline-delimited messages**.

For example:

```text
Message One\n
Message Two\n
Message Three\n
```

The server maintains a receive buffer and processes complete messages whenever a newline delimiter is received.

This allows the application to:

* Process multiple messages received in a single `recv()` call independently
* Handle partial messages that span multiple TCP packets
* Prevent multiple application messages from being incorrectly treated as a single message

## Wireshark Analysis

Wireshark was used to inspect the application's TCP traffic during LAN testing.

The traffic was filtered using:

```text
tcp.port == 8080
```

The testing captured:

* TCP three-way handshake
* Client-to-server TCP data
* Server acknowledgements
* Source and destination IP addresses
* TCP source and destination ports
* Application payload data

## TCP Three-Way Handshake

The TCP connection establishment follows the standard three-way handshake:

```text
Client                         Server

  | -------- SYN ------------> |
  | <------ SYN-ACK ---------- |
  | -------- ACK ------------> |
  |                            |
  | ===== TCP Connection ===== |
```

A captured connection showed:

```text
Client: 192.168.0.106:58998
             |
             | SYN
             ▼
Server: 192.168.0.114:8080
             |
             | SYN-ACK
             ▼
Client
             |
             | ACK
             ▼
Server
```

Application data was subsequently observed on the established TCP connection.

## Testing Performed

The final build was validated with:

* Server startup
* TCP client connection
* Username registration
* Normal messaging
* Multiple simultaneous clients
* `/users`
* Private messaging using `/msg`
* Duplicate username detection
* Offline-user private messaging
* Graceful `/quit`
* Peer disconnect notification
* Reconnection using the same username
* LAN communication
* TCP message framing
* Wireshark packet inspection

The final source was compiled using:

```bash
gcc -Wall -Wextra -pthread
```

The final build completed with **no compiler warnings or errors**.

## Future Improvements

Possible future enhancements include:

* Password-based authentication
* Persistent chat history
* File transfer
* Improved command-line interface
* Message timestamps
* Chat rooms
* TLS encryption
* Server-side logging
* Configuration file for the server port and settings

## Author

**Jagan Mohan Reddy Chinthalapalli**

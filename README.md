# csc-4770-pa4: Distributed Lock Service with Dynamic Naming

This project implements a distributed lock service using ZeroMQ and C++ to coordinate access to named shared resources across multiple clients.

***

## Build Instructions

The project requires a **C++17** compatible compiler (e.g., g++ 7+) and the ZeroMQ library (`libzmq`).

### Quick Build (Recommended)

Use the provided `Makefile` to compile both the `lock_server` and `lock_client` executables:

```bash
make
```

### Manual Build

The explicit compilation commands can also be used. The necessary flags (`-std=c++17`, `-pthread`, and `-lzmq`) are included:

```bash
# Compile Server
g++ -std=c++17 -pthread -o lock_server lock_server.cpp session_registry.cpp -lzmq

# Compile Client
g++ -std=c++17 -pthread -o lock_client lock_client.cpp file_io.cpp -lzmq
```

***

## System Cleanup

To remove the compiled binaries (`lock_server` and `lock_client`), use the `clean` target defined in the `Makefile`:

```bash
make clean
```

***

## Client Usage

The `lock_client` executable is run with the following command-line arguments:

```bash
./lock_client <resource> <mode> [content] [sleep_time]
```

| Argument | Description | Required | Options |
| :--- | :--- | :--- | :--- |
| `<resource>` | The dynamically named resource (e.g., a filename like `file_A`). | Yes | Any string |
| `<mode>` | The lock mode to acquire. | Yes | `WRITE` or `READ` |
| `[content]` | The string content to write to the resource file (only used for `WRITE` mode). | No | Any string |
| `[sleep_time]` | Time (in seconds) to sleep before performing the WRITE operation (only used for `WRITE` mode). | No | Integer (defaults to 0) |

***

## Example Usage

This example demonstrates concurrent access where a Reader client must block and wait for a Writer client to finish its critical section.

**Terminal 1**: Starts the server in the background.

```bash
$ ./lock_server &
[1] 11076
Lock Server started on tcp://*:5555
```

**Terminal 2**: Writer acquires lock for 5 seconds, performs write, then releases.

```bash
$ ./lock_client file_A WRITE "Hello Distributed World" 5
CONNECTING to lock server at tcp://localhost:5555
CREATING lock for resource: file_A
REQUESTING lock for resource: file_A
LOCKED file_A
Sleeping for 5 seconds before WRITE...
WRITING value to file_A: Hello Distributed World
RELEASING lock for resource: file_A
UNLOCKED file_A
```

**Terminal 3**: Reader attempts lock, blocks until Writer releases, then reads the written data.

```bash
$ ./lock_client file_A READ
CONNECTING to lock server at tcp://localhost:5555
REQUESTING lock for resource: file_A
LOCKED file_A
READING value from file_A: Hello Distributed World
RELEASING lock for resource: file_A
DELETING lock for resource: file_A
UNLOCKED file_A
```

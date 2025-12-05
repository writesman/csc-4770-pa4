# csc-4770-pa4

## Build Instructions

This project requires a C++23 compatible compiler (e.g., g++ 13+) and libzmq.

```bash
# Compile Server
g++ -std=c++23 -o lock_server lock_server.cpp -lzmq

# Compile Client
g++ -std=c++23 -o lock_client lock_client.cpp -lzmq
```

## Example Usage

1. Start the Server: `./lock_server`

2. Start a Client (Write Mode): `./lock_client resource_A WRITE "Data Payload" 5`

3. Start a Client (Read Mode): `./lock_client resource_A READ`

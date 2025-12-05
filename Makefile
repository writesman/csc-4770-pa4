CXX = g++
CXXFLAGS = -std=c++17 -pthread
LDFLAGS = -lzmq

# Source files for the server executable
SERVER_OBJS = lock_server.cpp session_registry.cpp

# Source files for the client executable
CLIENT_OBJS = lock_client.cpp file_io.cpp

all: lock_server lock_client

lock_server: $(SERVER_OBJS)
	$(CXX) $(CXXFLAGS) -o lock_server $^ $(LDFLAGS)

lock_client: $(CLIENT_OBJS)
	$(CXX) $(CXXFLAGS) -o lock_client $^ $(LDFLAGS)

clean:
	rm -f lock_server lock_client
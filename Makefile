CXX = g++
CXXFLAGS = -std=c++23 -pthread # Use -std=c++17 if c++23 fails
LDFLAGS = -lzmq

all: lock_server lock_client

lock_server: lock_server.cpp
	$(CXX) $(CXXFLAGS) -o lock_server lock_server.cpp $(LDFLAGS)

lock_client: lock_client.cpp
	$(CXX) $(CXXFLAGS) -o lock_client lock_client.cpp $(LDFLAGS)

clean:
	rm -f lock_server lock_client
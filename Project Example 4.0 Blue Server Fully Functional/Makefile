# Compiler and flags
CXX = g++
CXXFLAGS = -O2 -std=c++17 -I/usr/include/mysql/mysql

# Libraries
LIBS_COMMON = -lmariadb
LIBS_AUTH   = -lmariadb

# Targets
TARGETS = auth.cgi transactions.cgi trade.cgi listings.cgi

all: $(TARGETS)

auth.cgi: auth.cpp
	$(CXX) $(CXXFLAGS) $< -o $@ $(LIBS_AUTH)

transactions.cgi: transactions.cpp
	$(CXX) $(CXXFLAGS) $< -o $@ $(LIBS_COMMON)

trade.cgi: trade.cpp
	$(CXX) $(CXXFLAGS) $< -o $@ $(LIBS_COMMON)

listings.cgi: listings.cpp
	$(CXX) $(CXXFLAGS) $< -o $@ $(LIBS_COMMON)

clean:
	rm -f $(TARGETS)
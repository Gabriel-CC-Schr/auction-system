# Makefile for Auction Website
# This file compiles all C++ files into CGI executables

# Compiler to use
CXX = g++

# Compiler flags
# -std=c++11: Use C++ 11 standard
# -Wall: Enable all warnings
# -o: Specify output file name
CXXFLAGS = -std=c++11 -Wall

# MySQL library flags
# -lmysqlclient: Link with MySQL client library
MYSQL_LIBS = -lmysqlclient

# List all CGI targets (the programs we want to create)
# Now includes index.cgi and style.cgi
TARGETS = index.cgi style.cgi login.cgi listings.cgi trade.cgi transactions.cgi

# Default target - builds all CGI files
all: $(TARGETS)

# Rule to build index.cgi from index.cpp
# This is the home page, now dynamically generated
index.cgi: index.cpp
	$(CXX) $(CXXFLAGS) -o index.cgi index.cpp
	chmod 755 index.cgi

# Rule to build style.cgi from style.cpp
# This is the stylesheet, now dynamically generated
style.cgi: style.cpp
	$(CXX) $(CXXFLAGS) -o style.cgi style.cpp
	chmod 755 style.cgi

# Rule to build login.cgi from login.cpp
login.cgi: login.cpp
	$(CXX) $(CXXFLAGS) -o login.cgi login.cpp $(MYSQL_LIBS)
	chmod 755 login.cgi

# Rule to build listings.cgi from listings.cpp
listings.cgi: listings.cpp
	$(CXX) $(CXXFLAGS) -o listings.cgi listings.cpp $(MYSQL_LIBS)
	chmod 755 listings.cgi

# Rule to build trade.cgi from trade.cpp
trade.cgi: trade.cpp
	$(CXX) $(CXXFLAGS) -o trade.cgi trade.cpp $(MYSQL_LIBS)
	chmod 755 trade.cgi

# Rule to build transactions.cgi from transactions.cpp
transactions.cgi: transactions.cpp
	$(CXX) $(CXXFLAGS) -o transactions.cgi transactions.cpp $(MYSQL_LIBS)
	chmod 755 transactions.cgi

# Clean target - removes all compiled files
clean:
	rm -f $(TARGETS)

# Install target - sets up the database
install: all
	mysql -u allindragons -psnogardnilla_002 cs370_section2_allindragons < schema.sql
	@echo "Database setup complete!"
	@echo "Make sure all files are in public_html/cgi-bin/"
	@echo "Make sure .htaccess is configured properly"

# Help target - shows available commands
help:
	@echo "Available targets:"
	@echo "  make all      - Compile all CGI programs (default)"
	@echo "  make clean    - Remove all compiled files"
	@echo "  make install  - Compile and set up database"
	@echo "  make help     - Show this help message"

# Phony targets (not actual files)
.PHONY: all clean install help
# Makefile for Auction Website - Assignment 1
# Compiles CGI files for login and registration functionality

CXX = g++
CXXFLAGS = -std=c++11 -Wall
MYSQL_LIBS = -lmysqlclient

TARGETS = index.cgi style.cgi login.cgi

all: $(TARGETS)

index.cgi: index.cpp
	$(CXX) $(CXXFLAGS) -o index.cgi index.cpp $(MYSQL_LIBS)
	chmod 755 index.cgi

style.cgi: style.cpp
	$(CXX) $(CXXFLAGS) -o style.cgi style.cpp
	chmod 755 style.cgi

login.cgi: login.cpp
	$(CXX) $(CXXFLAGS) -o login.cgi login.cpp $(MYSQL_LIBS)
	chmod 755 login.cgi

clean:
	rm -f $(TARGETS)

install: all
	mysql -u allindragons -psnogardnilla_002 cs370_section2_allindragons < schema.sql
	@echo "Database setup complete!"

.PHONY: all clean install
CXX ?= g++
CXXSTD = -std=c++17
OPT = -O2

# ---- Set these if your headers/libs are in nonstandard places ----
# Examples (RHEL-like defaults):
#   make MYSQL_INC=/usr/include/mariadb MYSQL_LIBDIR=/usr/lib64 MYSQL_LIBS=-lmariadb
# Examples (Debian-like defaults):
#   make MYSQL_INC=/usr/include/mysql MYSQL_LIBDIR=/usr/lib/x86_64-linux-gnu MYSQL_LIBS=-lmysqlclient
MYSQL_INC ?=
MYSQL_LIBDIR ?=
MYSQL_LIBS ?= -lmysqlclient  # or -lmariadb

OPENSSL_LIBS ?= -lcrypto

CXXFLAGS = $(OPT) $(CXXSTD) -Iinclude $(if $(MYSQL_INC),-I$(MYSQL_INC))
LDFLAGS  = $(if $(MYSQL_LIBDIR),-L$(MYSQL_LIBDIR))
LIBS     = $(MYSQL_LIBS) $(OPENSSL_LIBS)

WEBROOT ?= $(HOME)/public_html
SRC_DIR = src

all: auth.cgi transactions.cgi trade.cgi listings.cgi

auth.cgi: $(SRC_DIR)/auth.cpp include/cgi_utils.hpp include/auction_utils.hpp
	$(CXX) $(CXXFLAGS) -o $@ $< $(LDFLAGS) $(LIBS)

transactions.cgi: $(SRC_DIR)/transactions.cpp include/cgi_utils.hpp
	$(CXX) $(CXXFLAGS) -o $@ $< $(LDFLAGS) $(LIBS)

trade.cgi: $(SRC_DIR)/trade.cpp include/cgi_utils.hpp include/auction_utils.hpp
	$(CXX) $(CXXFLAGS) -o $@ $< $(LDFLAGS) $(LIBS)

listings.cgi: $(SRC_DIR)/listings.cpp include/cgi_utils.hpp include/auction_utils.hpp
	$(CXX) $(CXXFLAGS) -o $@ $< $(LDFLAGS) $(LIBS)

install: all
	install -d $(WEBROOT)
	install -m 0755 auth.cgi $(WEBROOT)/auth.cgi
	install -m 0755 transactions.cgi $(WEBROOT)/transactions.cgi
	install -m 0755 trade.cgi $(WEBROOT)/trade.cgi
	install -m 0755 listings.cgi $(WEBROOT)/listings.cgi
	install -m 0644 public/index.html $(WEBROOT)/index.html
	install -m 0644 public/style.css $(WEBROOT)/style.css
	install -m 0644 schema.sql $(WEBROOT)/schema.sql
	install -m 0644 README.md $(WEBROOT)/README.md

clean:
	rm -f auth.cgi transactions.cgi trade.cgi listings.cgi
CXX = g++

CXXFLAGS = -std=c++11 -Wall

MYSQL_LIBS = -lmysqlclient

TARGETS = index.cgi style.cgi login.cgi

all: $(TARGETS)

index.cgi: index.cpp
	$(CXX) $(CXXFLAGS) -o index.cgi index.cpp
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
	@echo "Make sure all files are in public_html/cgi-bin/"
	@echo "Make sure .htaccess is configured properly"

help:
	@echo "Available targets:"
	@echo "  make all      - Compile all CGI programs (default)"
	@echo "  make clean    - Remove all compiled files"
	@echo "  make install  - Compile and set up database"
	@echo "  make help     - Show this help message"

.PHONY: all clean install help
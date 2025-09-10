need the mariadb connector put in src for function

make clean
make MYSQL_INC="src/mariadb-connector-c-3.4.7-src/mariadb-connector-c-local/include -Isrc/mariadb-connector-c-3.4.7-src/mariadb-connector-c-local/include/mariadb" \
     MYSQL_LIBDIR="src/mariadb-connector-c-3.4.7-src/mariadb-connector-c-local/lib/mariadb" \
     MYSQL_LIBS="-Wl,-rpath,src/mariadb-connector-c-3.4.7-src/mariadb-connector-c-local/lib/mariadb -lmariadb"

.htaccess
SetEnv DB_HOST 127.0.0.1
SetEnv DB_USER auction
SetEnv DB_PASS YourStrongPassword!
SetEnv DB_NAME auction_db
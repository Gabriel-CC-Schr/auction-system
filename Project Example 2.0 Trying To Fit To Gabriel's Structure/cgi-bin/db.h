#pragma once
#include <mariadb/mysql.h>
#include <string>

MYSQL* db_connect();

// Prepared statement bind helpers (callers must keep referenced values alive through execute)
MYSQL_BIND bind_in_str(const std::string& s, unsigned long& len);
MYSQL_BIND bind_in_longlong(long long v);
MYSQL_BIND bind_in_double(double v);
long fetch_one_long(MYSQL_STMT* stmt);

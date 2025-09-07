#include "db.h"
#include "config.h"

MYSQL* db_connect(){
    MYSQL* c = mysql_init(nullptr);
    if(!c) return nullptr;
    mysql_options(c, MYSQL_SET_CHARSET_NAME, "utf8mb4");
#if defined(MYSQL_OPT_SSL_CA)
    mysql_options(c, MYSQL_OPT_SSL_CA, DB_SSL_CA_FILE);
#endif
#if defined(MARIADB_BASE_VERSION) && defined(MARIADB_OPT_SSL_ENFORCE)
    { unsigned int on=1; mysql_options(c, MARIADB_OPT_SSL_ENFORCE, &on); }
#endif
#if defined(MYSQL_OPT_SSL_MODE)
#ifndef SSL_MODE_VERIFY_CA
#define SSL_MODE_VERIFY_CA 3
#endif
    { int mode = SSL_MODE_VERIFY_CA; mysql_options(c, MYSQL_OPT_SSL_MODE, &mode); }
#endif
#if defined(MYSQL_OPT_SSL_VERIFY_SERVER_CERT)
    { my_bool verify=1; mysql_options(c, MYSQL_OPT_SSL_VERIFY_SERVER_CERT, &verify); }
#endif
    if(!mysql_real_connect(c, DB_HOST, DB_USER, DB_PASS, DB_NAME, DB_PORT, nullptr, 0)) return nullptr;
    return c;
}

MYSQL_BIND bind_in_str(const std::string& s, unsigned long& len){ MYSQL_BIND b{}; b.buffer_type=MYSQL_TYPE_STRING; b.buffer=(void*)s.data(); len=(unsigned long)s.size(); b.length=&len; return b; }
MYSQL_BIND bind_in_longlong(long long v){ MYSQL_BIND b{}; b.buffer_type=MYSQL_TYPE_LONGLONG; b.buffer=&v; b.is_unsigned=0; return b; }
MYSQL_BIND bind_in_double(double v){ MYSQL_BIND b{}; b.buffer_type=MYSQL_TYPE_DOUBLE; b.buffer=&v; return b; }

long fetch_one_long(MYSQL_STMT* stmt){ long long out=-1; my_bool isnull=0; MYSQL_BIND rb{}; rb.buffer_type=MYSQL_TYPE_LONGLONG; rb.buffer=&out; rb.is_null=&isnull; if(mysql_stmt_bind_result(stmt,&rb)!=0) return -1; if(mysql_stmt_store_result(stmt)!=0) return -1; if(mysql_stmt_fetch(stmt)!=0 || isnull) return -1; return (long)out; }

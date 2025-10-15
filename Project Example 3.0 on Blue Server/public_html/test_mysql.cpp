#if __has_include(<mariadb/mysql.h>)
  #include <mariadb/mysql.h>
#elif __has_include(<mysql/mysql.h>)
  #include <mysql/mysql.h>
#else
  #include <mysql.h>
#endif
int main(){ MYSQL* c = mysql_init(nullptr); return c?0:0; }

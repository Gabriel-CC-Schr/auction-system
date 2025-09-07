// Global configuration (DB, TLS, sessions)
#pragma once

// Database
#define DB_HOST "localhost"
#define DB_USER "root"
#define DB_PASS ""
#define DB_NAME "auctiondb"
#define DB_PORT 3306

// TLS CA file used to verify MariaDB server certificate
#define DB_SSL_CA_FILE "C:/xampp/mysql/certs/ca.pem"

// Session idle timeout (sliding window)
#define IDLE_TIMEOUT_MINUTES 5

// Cookie name
#define SESSION_COOKIE_NAME "SESSION_ID"

// Utility macro
#define UNUSED(x) (void)(x)

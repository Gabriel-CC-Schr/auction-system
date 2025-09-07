#pragma once
#include <mariadb/mysql.h>
#include <string>

long current_user_and_slide(MYSQL* c); // -1 if not logged in/expired; on success extends expiry by IDLE_TIMEOUT_MINUTES
bool create_session(MYSQL* c, long long user_id, std::string& out_token);

bool user_exists(MYSQL* c, const std::string& email);
bool register_user(MYSQL* c, const std::string& email, const std::string& pass, long long& out_user_id, std::string& out_session_token);
bool login_user(MYSQL* c, const std::string& email, const std::string& pass, long long& out_user_id, std::string& out_session_token);

std::string make_set_cookie_header(const std::string& token); // Secure; HttpOnly; SameSite=Lax

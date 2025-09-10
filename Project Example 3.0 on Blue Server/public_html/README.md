# Minimal Auction (Refactored)

Neater layout with shared headers to avoid code duplication.

## Structure
```
minimal_auction_refactored/
  include/
    cgi_utils.hpp
    auction_utils.hpp
  src/
    auth.cpp
    transactions.cpp
    trade.cpp
    listings.cpp
  cgi/
    auth.cgi
    transactions.cgi
    trade.cgi
    listings.cgi
  public/
    index.html
    style.css
  schema.sql
  Makefile
```

## Build
```bash
sudo apt-get install g++ libmysqlclient-dev libssl-dev
make
chmod 755 cgi/*.cgi
```

## Deploy
Copy everything to `~/public_html/`. Place CGI wrappers in `~/public_html/` (or keep under `cgi/` and adjust URL/ScriptAlias). Simplest is to copy `*.cgi` to the web root alongside binaries.

Configure Apache env vars for DB:
```
SetEnv DB_HOST localhost
SetEnv DB_USER youruser
SetEnv DB_PASS yourpass
SetEnv DB_NAME auction_db
```

Create DB:
```bash
mysql -u root -p < schema.sql
```

Visit:
- `/auth.cgi`
- `/listings.cgi`
- `/trade.cgi`
- `/transactions.cgi`
```
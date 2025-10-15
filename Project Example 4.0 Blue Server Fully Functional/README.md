# Minimal Web Auction (Apache + CGI + MySQL + C++)

Unzip this folder directly into your web root as `public_html/`, compile the CGI, and you're good to go.

## Setup

```bash
# assuming /var/www/html/public_html
cd public_html

# Install deps (Debian/Ubuntu)
sudo apt-get update
sudo apt-get install -y g++ libmysqlclient-dev apache2

# Database
mysql -uroot -p <<'SQL'
SOURCE schema.sql;
CREATE USER IF NOT EXISTS 'webauction_user'@'localhost' IDENTIFIED BY 'change_me';
GRANT ALL PRIVILEGES ON webauction.* TO 'webauction_user'@'localhost';
FLUSH PRIVILEGES;
SQL

# (optional) seed data
mysql -uwebauction_user -p'change_me' webauction < seed.sql

# Build
make
chmod 755 *.cgi
```

## Apache

Allow CGI in `public_html` and allow the `.htaccess` to work:
```
<Directory /var/www/html/public_html>
  Options +ExecCGI
  AllowOverride Options,FileInfo
  Require all granted
  AddHandler cgi-script .cgi
</Directory>
```
Enable CGI & headers:
```bash
sudo a2enmod cgi headers
sudo systemctl restart apache2
```

## Notes
- Sessions expire after **5 minutes** of inactivity.
- Sellers cannot bid on their own items.
- All SQL executed via **prepared statements**.

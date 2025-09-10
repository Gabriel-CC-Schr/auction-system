-- same schema as before
CREATE DATABASE IF NOT EXISTS auction_db CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci;
USE auction_db;

CREATE TABLE users (
  id INT UNSIGNED AUTO_INCREMENT PRIMARY KEY,
  email VARCHAR(255) NOT NULL UNIQUE,
  password_hash VARBINARY(64) NOT NULL,
  password_salt VARBINARY(32) NOT NULL,
  created_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP
) ENGINE=InnoDB;

CREATE TABLE sessions (
  id CHAR(64) NOT NULL PRIMARY KEY,
  user_id INT UNSIGNED NOT NULL,
  last_activity DATETIME NOT NULL,
  created_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
  FOREIGN KEY (user_id) REFERENCES users(id) ON DELETE CASCADE,
  INDEX (user_id),
  INDEX (last_activity)
) ENGINE=InnoDB;

CREATE TABLE auctions (
  id INT UNSIGNED AUTO_INCREMENT PRIMARY KEY,
  seller_id INT UNSIGNED NOT NULL,
  title VARCHAR(255) NOT NULL,
  description TEXT NOT NULL,
  starting_price DECIMAL(10,2) NOT NULL CHECK (starting_price >= 0),
  start_time DATETIME NOT NULL,
  end_time DATETIME NOT NULL,
  status ENUM('open','closed') NOT NULL DEFAULT 'open',
  created_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
  FOREIGN KEY (seller_id) REFERENCES users(id) ON DELETE CASCADE,
  INDEX (seller_id),
  INDEX (status),
  INDEX (end_time)
) ENGINE=InnoDB;

CREATE TABLE bids (
  id INT UNSIGNED AUTO_INCREMENT PRIMARY KEY,
  auction_id INT UNSIGNED NOT NULL,
  bidder_id INT UNSIGNED NOT NULL,
  amount DECIMAL(10,2) NOT NULL CHECK (amount >= 0),
  created_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
  FOREIGN KEY (auction_id) REFERENCES auctions(id) ON DELETE CASCADE,
  FOREIGN KEY (bidder_id) REFERENCES users(id) ON DELETE CASCADE,
  INDEX (auction_id),
  INDEX (bidder_id),
  INDEX (created_at)
) ENGINE=InnoDB;

CREATE TABLE transactions (
  id INT UNSIGNED AUTO_INCREMENT PRIMARY KEY,
  auction_id INT UNSIGNED NOT NULL UNIQUE,
  buyer_id INT UNSIGNED NOT NULL,
  final_price DECIMAL(10,2) NOT NULL,
  created_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
  FOREIGN KEY (auction_id) REFERENCES auctions(id) ON DELETE CASCADE,
  FOREIGN KEY (buyer_id) REFERENCES users(id) ON DELETE CASCADE,
  INDEX (buyer_id)
) ENGINE=InnoDB;

CREATE OR REPLACE VIEW auction_highest_bid AS
SELECT b.auction_id,
       MAX(b.amount) AS highest_bid
FROM bids b
GROUP BY b.auction_id;

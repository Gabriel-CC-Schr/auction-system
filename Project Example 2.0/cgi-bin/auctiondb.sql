DROP DATABASE IF EXISTS auctiondb;
CREATE DATABASE auctiondb CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci;
USE auctiondb;

CREATE DATABASE IF NOT EXISTS auctiondb
  CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci;
USE auctiondb;

-- USERS
CREATE TABLE users (
  user_id       INT AUTO_INCREMENT PRIMARY KEY,
  email         VARCHAR(255) NOT NULL UNIQUE,
  password_salt CHAR(32)     NOT NULL,      -- hex salt
  password_hash CHAR(64)     NOT NULL,      -- hex SHA-256(salt || password)
  created_at    DATETIME     NOT NULL DEFAULT CURRENT_TIMESTAMP
) ENGINE=InnoDB;

-- SESSIONS
CREATE TABLE sessions (
  session_id CHAR(64) PRIMARY KEY,          -- hex token
  user_id    INT       NOT NULL,
  created_at DATETIME  NOT NULL DEFAULT CURRENT_TIMESTAMP,
  expires_at DATETIME  NOT NULL,
  CONSTRAINT fk_sessions_user
    FOREIGN KEY (user_id) REFERENCES users(user_id)
    ON DELETE CASCADE,
  INDEX (user_id),
  INDEX (expires_at)
) ENGINE=InnoDB;

-- ITEMS
CREATE TABLE items (
  item_id        INT AUTO_INCREMENT PRIMARY KEY,
  seller_id      INT          NOT NULL,
  title          VARCHAR(150) NOT NULL,
  description    TEXT         NOT NULL,
  starting_price DECIMAL(10,2) NOT NULL CHECK (starting_price >= 0),
  created_at     DATETIME     NOT NULL DEFAULT CURRENT_TIMESTAMP,
  CONSTRAINT fk_items_seller
    FOREIGN KEY (seller_id) REFERENCES users(user_id)
    ON DELETE CASCADE,
  INDEX (seller_id)
) ENGINE=InnoDB;

-- AUCTIONS (one active/finished auction per item)
CREATE TABLE auctions (
  auction_id INT AUTO_INCREMENT PRIMARY KEY,
  item_id    INT        NOT NULL UNIQUE,
  start_time DATETIME   NOT NULL,
  end_time   DATETIME   NOT NULL,
  closed     TINYINT(1) NOT NULL DEFAULT 0,
  CONSTRAINT fk_auctions_item
    FOREIGN KEY (item_id) REFERENCES items(item_id)
    ON DELETE CASCADE,
  INDEX (end_time),
  INDEX (start_time)
) ENGINE=InnoDB;

-- BIDS (max-bid style simplified: amount is the user's current max bid)
CREATE TABLE bids (
  bid_id     INT AUTO_INCREMENT PRIMARY KEY,
  auction_id INT        NOT NULL,
  bidder_id  INT        NOT NULL,
  amount     DECIMAL(10,2) NOT NULL CHECK (amount >= 0),
  created_at DATETIME   NOT NULL DEFAULT CURRENT_TIMESTAMP,
  CONSTRAINT fk_bids_auction
    FOREIGN KEY (auction_id) REFERENCES auctions(auction_id)
    ON DELETE CASCADE,
  CONSTRAINT fk_bids_bidder
    FOREIGN KEY (bidder_id) REFERENCES users(user_id)
    ON DELETE CASCADE,
  INDEX (auction_id),
  INDEX (bidder_id),
  INDEX (auction_id, amount)
) ENGINE=InnoDB;
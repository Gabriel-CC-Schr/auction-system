-- schema.sql — MySQL (= 8.0) - Updated for C++ compatibility
CREATE DATABASE IF NOT EXISTS cs370_section2_allindragons CHARACTER SET utf8mb4 COLLATE utf8mb4_general_ci;
USE cs370_section2_allindragons;

CREATE TABLE IF NOT EXISTS users (
  id BIGINT UNSIGNED PRIMARY KEY AUTO_INCREMENT,
  email VARCHAR(255) NOT NULL UNIQUE,
  -- Changed to CHAR(56) to match SHA2-224 output (28 bytes * 2 hex chars = 56)
  password_hash CHAR(56) NOT NULL,
  created_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP
) ENGINE=InnoDB;

CREATE TABLE IF NOT EXISTS auctions (
  id BIGINT UNSIGNED PRIMARY KEY AUTO_INCREMENT,
  seller_id BIGINT UNSIGNED NOT NULL,
  title VARCHAR(120) NOT NULL,
  -- Changed to VARCHAR(1024) to match C++ buffer size
  description VARCHAR(1024) NOT NULL,
  start_price DECIMAL(12,2) NOT NULL,
  start_time DATETIME NOT NULL,
  end_time   DATETIME NOT NULL,
  status ENUM('OPEN','CLOSED') NOT NULL DEFAULT 'OPEN',
  created_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
  CONSTRAINT fk_auctions_seller FOREIGN KEY (seller_id) REFERENCES users(id) ON DELETE CASCADE,
  INDEX idx_auctions_end_time (status, end_time),
  INDEX idx_auctions_status_active (status, end_time), -- Additional index for C++ queries
  CHECK (end_time > start_time)
) ENGINE=InnoDB;

CREATE TABLE IF NOT EXISTS bids (
  id BIGINT UNSIGNED PRIMARY KEY AUTO_INCREMENT,
  auction_id BIGINT UNSIGNED NOT NULL,
  bidder_id BIGINT UNSIGNED NOT NULL,
  amount DECIMAL(12,2) NOT NULL,
  created_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
  CONSTRAINT fk_bids_auction FOREIGN KEY (auction_id) REFERENCES auctions(id) ON DELETE CASCADE,
  CONSTRAINT fk_bids_bidder FOREIGN KEY (bidder_id) REFERENCES users(id) ON DELETE CASCADE,
  INDEX idx_bids_auction_created (auction_id, created_at DESC),
  INDEX idx_bids_auction_amount (auction_id, amount DESC), -- Additional index for MAX(amount) queries
  UNIQUE KEY unique_bid_per_user (auction_id, bidder_id, amount), -- Prevent duplicate bids
  CHECK (amount > 0)
) ENGINE=InnoDB;

CREATE TABLE IF NOT EXISTS sessions (
  id CHAR(64) PRIMARY KEY, -- Perfect match for 64-character hex tokens
  user_id BIGINT UNSIGNED NOT NULL,
  last_active DATETIME NOT NULL,
  created_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
  CONSTRAINT fk_sessions_user FOREIGN KEY (user_id) REFERENCES users(id) ON DELETE CASCADE,
  INDEX idx_sessions_last_active (last_active)
) ENGINE=InnoDB;

-- View remains the same - it's perfect as-is
CREATE OR REPLACE VIEW v_user_purchases AS
SELECT b.bidder_id   AS user_id,
       a.id          AS auction_id,
       a.title,
       a.description,
       MAX(b.amount) AS winning_amount
FROM auctions a
JOIN bids b ON b.auction_id = a.id
WHERE a.status = 'CLOSED'
GROUP BY b.bidder_id, a.id, a.title, a.description;
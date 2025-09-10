
#ifndef AUCTION_UTILS_HPP
#define AUCTION_UTILS_HPP

#include "cgi_utils.hpp"

namespace mini {

inline double current_price(DB& db, int auction_id){
  std::string q = "SELECT IFNULL((SELECT MAX(amount) FROM bids WHERE auction_id="+std::to_string(auction_id)+"), "
                  "(SELECT starting_price FROM auctions WHERE id="+std::to_string(auction_id)+"))";
  db.exec(q); MYSQL_RES* r=mysql_store_result(db.conn); MYSQL_ROW row=mysql_fetch_row(r);
  double v = row? atof(row[0]) : 0.0; mysql_free_result(r); return v;
}

inline void close_expired(DB& db){
  db.exec("UPDATE auctions SET status='closed' WHERE status='open' AND end_time<=UTC_TIMESTAMP()");
  db.exec("INSERT IGNORE INTO transactions(auction_id,buyer_id,final_price,created_at) "
          "SELECT a.id, b.bidder_id, b.amount, UTC_TIMESTAMP() "
          "FROM auctions a JOIN bids b ON b.auction_id=a.id "
          "LEFT JOIN transactions t ON t.auction_id=a.id "
          "WHERE a.status='closed' AND t.auction_id IS NULL AND b.amount=(SELECT MAX(b2.amount) FROM bids b2 WHERE b2.auction_id=a.id)");
}

inline std::string select_open_auctions(DB& db, int user_id){
  std::string html = "<select name='auction_id' required>";
  std::string q = "SELECT id,title FROM auctions WHERE status='open' AND seller_id<>"+std::to_string(user_id)+" AND end_time>UTC_TIMESTAMP() ORDER BY end_time ASC";
  db.exec(q); MYSQL_RES* r=mysql_store_result(db.conn); MYSQL_ROW row;
  while((row=mysql_fetch_row(r))){
    html += "<option value='"+std::string(row[0])+"'>"+std::string(row[1])+"</option>";
  }
  mysql_free_result(r); html += "</select>";
  return html;
}

} // namespace mini
#endif

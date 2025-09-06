// ============================================
// Transactions.java — user transactions dashboard (CGI)
// ============================================

import java.io.*;
import java.sql.*;
import java.util.*;

class Transactions {
    static final String DB_HOST = "localhost";
    static final int    DB_PORT = 3306;
    static final String DB_NAME = "auctiondb";
    static final String DB_USER = "root";
    static final String DB_PASS = "";

    static Connection getConnection() throws Exception {
        String mariadbUrl = "jdbc:mariadb://"+DB_HOST+":"+DB_PORT+"/"+DB_NAME+"?useSSL=false";
        String mysqlUrl   = "jdbc:mysql://"+DB_HOST+":"+DB_PORT+"/"+DB_NAME+
                "?useSSL=false&requireSSL=false&allowPublicKeyRetrieval=true&serverTimezone=UTC";
        try { Class.forName("org.mariadb.jdbc.Driver"); return DriverManager.getConnection(mariadbUrl, DB_USER, DB_PASS);} 
        catch (ClassNotFoundException ignore) { Class.forName("com.mysql.cj.jdbc.Driver"); return DriverManager.getConnection(mysqlUrl, DB_USER, DB_PASS);}    }

    static void header(){ System.out.print("Content-Type: text/html\r\n\r\n"); }
    static void top(){ System.out.println("<!doctype html><html><head><meta charset='utf-8'><title>Your Transactions</title><style>body{font-family:sans-serif;max-width:1000px;margin:24px auto;padding:0 12px}h2{margin-top:28px}table{border-collapse:collapse;width:100%}th,td{border:1px solid #ddd;padding:8px}.warn{color:#b00;font-weight:bold}</style></head><body><h1>Your Transactions</h1><p><a href='/cgi-bin/listings.cgi'>All open auctions</a> · <a href='/cgi-bin/bid_sell.cgi'>Bid / Sell</a></p><hr>"); }
    static void bottom(){ System.out.println("</body></html>"); }

    static long currentUser(Connection cx) throws SQLException {
        String cookies = System.getenv("HTTP_COOKIE"); if(cookies==null) return -1;
        String key="SESSION_ID="; int p=cookies.indexOf(key); if(p<0) return -1; int s=p+key.length(); int e=cookies.indexOf(';', s); if(e<0) e=cookies.length();
        String token=cookies.substring(s,e);
        try(PreparedStatement ps=cx.prepareStatement("SELECT user_id FROM sessions WHERE session_id=? AND expires_at>NOW() LIMIT 1")){
            ps.setString(1, token); try(ResultSet rs=ps.executeQuery()){ if(rs.next()) return rs.getLong(1);} }
        return -1;
    }

    static void runAndPrintTable(Connection cx, String sql, List<String> headers) throws SQLException{
        try(Statement st=cx.createStatement(); ResultSet rs=st.executeQuery(sql)){
            ResultSetMetaData md=rs.getMetaData(); int ncols=md.getColumnCount();
            boolean any=false; StringBuilder sb=new StringBuilder(); sb.append("<table><thead><tr>");
            for(String h: headers) sb.append("<th").append(">").append(h).append("</th>");
            sb.append("</tr></thead><tbody>");
            while(rs.next()){
                any=true; sb.append("<tr>");
                for(int i=1;i<=ncols;i++){ String v=rs.getString(i); if(v==null) v=""; sb.append("<td>").append(v).append("</td>"); }
                sb.append("</tr>");
            }
            sb.append("</tbody></table>");
            if(any) System.out.println(sb.toString()); else System.out.println("<p><em>None.</em></p>");
        } catch(SQLException e){ System.out.println("<p class='warn'>Query failed.</p>"); }
    }

    public static void main(String[] args) throws Exception{
        header(); top();
        try(Connection cx=getConnection()){
            if(cx==null){ System.out.println("<p class='warn'>DB connection failed.</p>"); bottom(); return; }
            long uid = currentUser(cx);
            if(uid<0){ System.out.println("<p class='warn'>You are not logged in. <a href='/cgi-bin/auth.cgi'>Log in</a></p>"); bottom(); return; }

            System.out.println("<h2>1. Selling — Active</h2>");
            runAndPrintTable(cx,
                "SELECT i.title, FORMAT(i.starting_price,2), DATE_FORMAT(a.end_time,'%Y-%m-%d %H:%i:%s'), "+
                "FORMAT(GREATEST(IFNULL((SELECT MAX(b.amount) FROM bids b WHERE b.auction_id=a.auction_id),0), i.starting_price),2) "+
                "FROM items i JOIN auctions a ON a.item_id=i.item_id WHERE i.seller_id="+uid+" AND a.end_time>NOW() AND a.closed=0 ORDER BY a.end_time ASC",
                Arrays.asList("Item","Starting Price","Ends","Current Highest")
            );

            System.out.println("<h2>1. Selling — Sold</h2>");
            runAndPrintTable(cx,
                "SELECT i.title, DATE_FORMAT(a.end_time,'%Y-%m-%d %H:%i:%s'), "+
                "FORMAT(IFNULL((SELECT MAX(b.amount) FROM bids b WHERE b.auction_id=a.auction_id), i.starting_price),2) "+
                "FROM items i JOIN auctions a ON a.item_id=i.item_id WHERE i.seller_id="+uid+" AND a.end_time<=NOW() ORDER BY a.end_time DESC",
                Arrays.asList("Item","Ended","Winning Bid")
            );

            System.out.println("<h2>2. Purchases</h2>");
            runAndPrintTable(cx,
                "SELECT i.title, DATE_FORMAT(a.end_time,'%Y-%m-%d %H:%i:%s'), FORMAT(m.max_amt,2) "+
                "FROM auctions a JOIN items i ON i.item_id=a.item_id "+
                "JOIN (SELECT auction_id, MAX(amount) AS max_amt FROM bids GROUP BY auction_id) m ON m.auction_id=a.auction_id "+
                "JOIN bids b ON b.auction_id=a.auction_id AND b.amount=m.max_amt "+
                "WHERE a.end_time<=NOW() AND b.bidder_id="+uid+" ORDER BY a.end_time DESC",
                Arrays.asList("Item","Ended","Your Winning Bid")
            );

            System.out.println("<h2>3. Current Bids</h2><p>Click “Increase Max Bid” to raise your bid.</p>");
            runAndPrintTable(cx,
                "SELECT i.title, DATE_FORMAT(a.end_time,'%Y-%m-%d %H:%i:%s'), "+
                "FORMAT(ub.max_amt,2), "+
                "FORMAT(GREATEST(IFNULL(allb.max_all,0), i.starting_price),2), "+
                "CASE WHEN ub.max_amt >= GREATEST(IFNULL(allb.max_all,0), i.starting_price) AND ub.max_amt = allb.max_all THEN 'Leading' ELSE 'Outbid' END, "+
                "CONCAT('<a href\\''/cgi-bin/bid_sell.cgi?mode=bid&auction_id=', a.auction_id, '\\'>Increase Max Bid</a>') "+
                "FROM auctions a JOIN items i ON i.item_id=a.item_id "+
                "JOIN (SELECT auction_id, bidder_id, MAX(amount) AS max_amt FROM bids WHERE bidder_id="+uid+" GROUP BY auction_id, bidder_id) ub ON ub.auction_id=a.auction_id "+
                "LEFT JOIN (SELECT auction_id, MAX(amount) AS max_all FROM bids GROUP BY auction_id) allb ON allb.auction_id=a.auction_id "+
                "WHERE a.end_time>NOW() AND a.closed=0 ORDER BY a.end_time ASC",
                Arrays.asList("Item","Ends","Your Max Bid","Current Highest","Status","Action")
            );

            System.out.println("<h2>4. Didn’t Win</h2>");
            runAndPrintTable(cx,
                "SELECT i.title, DATE_FORMAT(a.end_time,'%Y-%m-%d %H:%i:%s'), FORMAT(m.max_amt,2) "+
                "FROM auctions a JOIN items i ON i.item_id=a.item_id "+
                "JOIN (SELECT auction_id, MAX(amount) AS max_amt FROM bids GROUP BY auction_id) m ON m.auction_id=a.auction_id "+
                "LEFT JOIN (SELECT auction_id, bidder_id, MAX(amount) AS mymax FROM bids WHERE bidder_id="+uid+" GROUP BY auction_id, bidder_id) me ON me.auction_id=a.auction_id "+
                "WHERE a.end_time<=NOW() AND (me.mymax IS NULL OR me.mymax < m.max_amt) ORDER BY a.end_time DESC",
                Arrays.asList("Item","Ended","Winning Bid")
            );
        }
        bottom();
    }
}

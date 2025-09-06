// ============================================
// Listings.java — show open auctions (CGI)
// ============================================

import java.io.*;
import java.net.URLDecoder;
import java.nio.charset.StandardCharsets;
import java.sql.*;

class Listings {
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

    static String html(String s){ if(s==null) return ""; return s.replace("&","&amp;").replace("<","&lt;").replace(">","&gt;").replace("\"","&quot;").replace("'","&#39;"); }

    static long currentUser(Connection cx) throws SQLException {
        String cookies = System.getenv("HTTP_COOKIE");
        if(cookies==null) return -1;
        String key="SESSION_ID="; int p=cookies.indexOf(key); if(p<0) return -1; int s=p+key.length(); int e=cookies.indexOf(';', s); if(e<0) e=cookies.length();
        String token = cookies.substring(s,e);
        try(PreparedStatement ps=cx.prepareStatement("SELECT user_id FROM sessions WHERE session_id=? AND expires_at>NOW() LIMIT 1")){
            ps.setString(1, token); try(ResultSet rs=ps.executeQuery()){ if(rs.next()) return rs.getLong(1);} }
        return -1;
    }

    static void header(){ System.out.print("Content-Type: text/html\r\n\r\n"); }
    static void top(){
        System.out.println("<!doctype html><html><head><meta charset='utf-8'><title>Open Auctions</title><style>body{font-family:sans-serif;max-width:1100px;margin:24px auto;padding:0 12px}.card{border:1px solid #ddd;border-radius:12px;padding:12px;margin:12px 0}.grid{display:grid;grid-template-columns:2fr 1fr 1fr;gap:8px}button,a.button{border:1px solid #ccc;border-radius:10px;padding:8px 12px;display:inline-block;text-decoration:none}small{color:#666}</style></head><body><h1>Open Auctions</h1><p><a href='/cgi-bin/auth.cgi'>Login/Register</a> · <a href='/cgi-bin/bid_sell.cgi'>Bid / Sell</a> · <a href='/cgi-bin/transactions.cgi'>Your transactions</a></p><hr>");
    }
    static void bottom(){ System.out.println("</body></html>"); }

    public static void main(String[] args) throws Exception{
        header(); top();
        try(Connection cx=getConnection()){
            if(cx==null){ System.out.println("<p style='color:#b00'>DB connection failed.</p>"); bottom(); return; }
            long uid = currentUser(cx);
            String sql = "SELECT a.auction_id, i.title, i.description, "+
                    "FORMAT(i.starting_price,2) AS startp, "+
                    "FORMAT(GREATEST(IFNULL((SELECT MAX(b.amount) FROM bids b WHERE b.auction_id=a.auction_id),0), i.starting_price),2) AS currentp, "+
                    "DATE_FORMAT(a.end_time,'%Y-%m-%d %H:%i:%s') AS ends_at, i.seller_id "+
                    "FROM auctions a JOIN items i ON i.item_id=a.item_id "+
                    "WHERE a.end_time>NOW() AND a.closed=0 ORDER BY a.end_time ASC";
            try(Statement st=cx.createStatement(); ResultSet rs=st.executeQuery(sql)){
                boolean any=false;
                while(rs.next()){
                    any=true;
                    String aid   = rs.getString(1);
                    String title = rs.getString(2);
                    String descr = rs.getString(3);
                    String startp= rs.getString(4);
                    String curp  = rs.getString(5);
                    String ends  = rs.getString(6);
                    long seller  = rs.getLong(7);
                    System.out.print("<div class='card'><div class='grid'>");
                    System.out.print("<div><h3>"+html(title)+"</h3><p>"+html(descr)+"</p><small>Ends: "+ends+"</small></div>");
                    System.out.print("<div><strong>Starting</strong><br>$"+startp+"</div>");
                    System.out.print("<div><strong>Current</strong><br>$"+curp+"</div></div>");
                    if(uid>0 && uid!=seller){
                        System.out.print("<p><a class='button' href='/cgi-bin/bid_sell.cgi?mode=bid&auction_id="+aid+"'>Bid</a></p>");
                    } else if(uid==seller){
                        System.out.print("<p><small>You are the seller.</small></p>");
                    } else {
                        System.out.print("<p><small><a class='button' href='/cgi-bin/auth.cgi'>Log in to bid</a></small></p>");
                    }
                    System.out.println("</div>");
                }
                if(!any) System.out.println("<p><em>No open auctions right now.</em></p>");
            }
        }
        bottom();
    }
}
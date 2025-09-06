// ============================================
// BidSell.java — bid on items / create auctions (CGI)
// ============================================

import java.io.*;
import java.net.URLDecoder;
import java.nio.charset.StandardCharsets;
import java.sql.*;
import java.util.*;

class BidSell {
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

    static Map<String,String> parseKV(String qs){ Map<String,String> m=new LinkedHashMap<>(); if(qs==null) return m; for(String p: qs.split("&")){ if(p.isEmpty()) continue; String[] kv=p.split("=",2); String k=URLDecoder.decode(kv[0], StandardCharsets.UTF_8); String v=kv.length>1?URLDecoder.decode(kv[1], StandardCharsets.UTF_8):""; m.put(k,v);} return m; }

    static String readBody() throws IOException{ String cl=System.getenv("CONTENT_LENGTH"); int n=(cl==null||cl.isEmpty())?0:Integer.parseInt(cl); if(n<=0) return ""; byte[] buf=new byte[n]; int off=0; while(off<n){ int r=System.in.read(buf, off, n-off); if(r<0) break; off+=r;} return new String(buf,0,off, StandardCharsets.UTF_8);}    

    static void header(){ System.out.print("Content-Type: text/html\r\n\r\n"); }
    static void top(){ System.out.println("<!doctype html><html><head><meta charset='utf-8'><title>Bid / Sell</title><style>body{font-family:sans-serif;max-width:1000px;margin:24px auto;padding:0 12px}form{margin:18px 0;padding:12px;border:1px solid #ddd;border-radius:10px}input,select,button,textarea{padding:8px;margin:6px 0}label{display:block;margin-top:8px}table{border-collapse:collapse;width:100%}th,td{border:1px solid #ddd;padding:8px}</style></head><body><h1>Bid / Sell</h1><p><a href='/cgi-bin/listings.cgi'>All open auctions</a> · <a href='/cgi-bin/transactions.cgi'>Your transactions</a></p><hr>"); }
    static void bottom(){ System.out.println("</body></html>"); }

    static long currentUser(Connection cx) throws SQLException {
        String cookies = System.getenv("HTTP_COOKIE"); if(cookies==null) return -1;
        String key="SESSION_ID="; int p=cookies.indexOf(key); if(p<0) return -1; int s=p+key.length(); int e=cookies.indexOf(';', s); if(e<0) e=cookies.length();
        String token=cookies.substring(s,e);
        try(PreparedStatement ps=cx.prepareStatement("SELECT user_id FROM sessions WHERE session_id=? AND expires_at>NOW() LIMIT 1")){
            ps.setString(1, token); try(ResultSet rs=ps.executeQuery()){ if(rs.next()) return rs.getLong(1);} }
        return -1;
    }

    static void showForms(Connection cx, long uid, String preselect) throws SQLException {
        System.out.println("<h2>Bid on an Item</h2><form method='POST'><input type='hidden' name='action' value='bid'>");
        System.out.print("<label>Item:</label><select name='auction_id' required>");
        String sql = "SELECT a.auction_id, i.title, FORMAT(GREATEST(IFNULL((SELECT MAX(b.amount) FROM bids b WHERE b.auction_id=a.auction_id),0), i.starting_price),2) AS cur " +
                     "FROM auctions a JOIN items i ON i.item_id=a.item_id WHERE a.end_time>NOW() AND a.closed=0 AND i.seller_id<>? ORDER BY a.end_time ASC";
        try(PreparedStatement ps=cx.prepareStatement(sql)){
            ps.setLong(1, uid);
            try(ResultSet rs=ps.executeQuery()){
                while(rs.next()){
                    String aid=rs.getString(1), title=rs.getString(2), cur=rs.getString(3);
                    System.out.print("<option value='"+aid+"' "+(aid.equals(preselect)?"selected":"")+">");
                    System.out.print(html(title)+" — Current: $"+cur+"</option>");
                }
            }
        }
        System.out.println("</select><label>Your Maximum Bid (USD):</label><input type='number' name='amount' min='0' step='0.01' required><button type='submit'>Place Bid</button></form>");

        System.out.println("<h2>Sell an Item</h2><form method='POST'><input type='hidden' name='action' value='sell'>"+
                "<label>Title</label><input type='text' name='title' maxlength='150' required>"+
                "<label>Description</label><textarea name='description' rows='4' required></textarea>"+
                "<label>Starting Price (USD)</label><input type='number' name='starting_price' min='0' step='0.01' required>"+
                "<label>Start Date & Time (YYYY-MM-DD HH:MM:SS)</label><input type='text' name='start_time' placeholder='2025-09-04 12:00:00' required>"+
                "<p><em>All auctions last exactly 168 hours (7 days).</em></p>"+
                "<button type='submit'>Create Auction</button></form>");
    }

    public static void main(String[] args) throws Exception{
        header(); top();
        try(Connection cx=getConnection()){
            if(cx==null){ System.out.println("<p style='color:#b00'>DB connection failed.</p>"); bottom(); return; }
            long uid = currentUser(cx);
            if(uid<0){ System.out.println("<p style='color:#b00'>You are not logged in. <a href='/cgi-bin/auth.cgi'>Log in</a></p>"); bottom(); return; }

            String method = System.getenv("REQUEST_METHOD");
            if(method==null || method.equalsIgnoreCase("GET")){
                String qs = System.getenv("QUERY_STRING");
                Map<String,String> kv = parseKV(qs==null?"":qs);
                String pre = kv.getOrDefault("auction_id", "");
                showForms(cx, uid, pre);
                bottom();
                return;
            }

            // POST
            Map<String,String> kv = parseKV(readBody());
            String action = kv.getOrDefault("action","");
            if("bid".equals(action)){
                String auctionId = kv.getOrDefault("auction_id","");
                String amountStr = kv.getOrDefault("amount","");
                if(auctionId.isEmpty()||amountStr.isEmpty()){ System.out.println("<p>Missing fields.</p>"); showForms(cx,uid,""); bottom(); return; }

                // Load auction + item
                long seller; double starting, current; int closed;
                try(PreparedStatement ps=cx.prepareStatement(
                        "SELECT a.auction_id, i.item_id, i.seller_id, i.starting_price, "+
                        "GREATEST(IFNULL((SELECT MAX(b.amount) FROM bids b WHERE b.auction_id=a.auction_id),0), i.starting_price) AS cur, "+
                        "a.end_time, a.closed FROM auctions a JOIN items i ON i.item_id=a.item_id WHERE a.auction_id=? LIMIT 1")){
                    ps.setString(1, auctionId);
                    try(ResultSet rs=ps.executeQuery()){
                        if(!rs.next()){ System.out.println("<p>Invalid auction.</p>"); showForms(cx,uid,auctionId); bottom(); return; }
                        seller = rs.getLong(3); starting = rs.getDouble(4); current = rs.getDouble(5); closed = rs.getInt(7);
                    }
                }
                if(seller==uid){ System.out.println("<p style='color:#b00'>You cannot bid on your own item.</p>"); showForms(cx,uid,auctionId); bottom(); return; }
                if(closed!=0){ System.out.println("<p style='color:#b00'>Auction is closed.</p>"); showForms(cx,uid,auctionId); bottom(); return; }
                double amt;
                try{ amt = Double.parseDouble(amountStr); } catch(Exception e){ System.out.println("<p>Invalid amount.</p>"); showForms(cx,uid,auctionId); bottom(); return; }
                if(amt < starting || amt <= current){
                    System.out.println("<p style='color:#b00'>Your max bid must be ≥ starting price ($"+starting+") and > current highest ($"+current+").</p>");
                    showForms(cx,uid,auctionId); bottom(); return;
                }
                try(PreparedStatement ps=cx.prepareStatement("INSERT INTO bids(auction_id,bidder_id,amount) VALUES(?,?,?)")){
                    ps.setString(1, auctionId); ps.setLong(2, uid); ps.setDouble(3, amt); ps.executeUpdate();
                }
                System.out.println("<p>Bid placed successfully.</p>");
                showForms(cx, uid, auctionId); bottom(); return;
            }
            else if("sell".equals(action)){
                String title=kv.getOrDefault("title",""), descr=kv.getOrDefault("description",""), sp=kv.getOrDefault("starting_price",""), st=kv.getOrDefault("start_time","");
                if(title.isEmpty()||descr.isEmpty()||sp.isEmpty()||st.isEmpty()){ System.out.println("<p>Missing fields.</p>"); showForms(cx,uid,""); bottom(); return; }
                long itemId;
                try(PreparedStatement ps=cx.prepareStatement("INSERT INTO items(seller_id,title,description,starting_price) VALUES(?,?,?,?)", Statement.RETURN_GENERATED_KEYS)){
                    ps.setLong(1, uid); ps.setString(2, title); ps.setString(3, descr); ps.setBigDecimal(4, new java.math.BigDecimal(sp));
                    ps.executeUpdate(); try(ResultSet gk=ps.getGeneratedKeys()){ if(!gk.next()){ System.out.println("<p style='color:#b00'>Failed to create item.</p>"); showForms(cx,uid,""); bottom(); return;} itemId=gk.getLong(1);} }
                try(PreparedStatement ps=cx.prepareStatement("INSERT INTO auctions(item_id,start_time,end_time) VALUES(?, ?, DATE_ADD(?, INTERVAL 168 HOUR))")){
                    ps.setLong(1, itemId); ps.setString(2, st); ps.setString(3, st); ps.executeUpdate();
                } catch(SQLException e){ System.out.println("<p style='color:#b00'>Failed to create auction (check datetime format).</p>"); showForms(cx,uid,""); bottom(); return; }
                System.out.println("<p>Auction created! It will end 7 days after <strong>"+html(st)+"</strong>.</p>");
                showForms(cx,uid,""); bottom(); return;
            }
            else{
                System.out.println("<p>Unknown action.</p>"); showForms(cx,uid,""); bottom(); return;
            }
        }
    }
}

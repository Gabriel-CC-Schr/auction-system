// ============================================
// Auth.java — login & registration (CGI)
// ============================================

import java.io.*;
import java.math.*;
import java.net.URLDecoder;
import java.nio.charset.StandardCharsets;
import java.security.SecureRandom;
import java.sql.*;
import java.util.*;

class Auth {
    // --- DB config ---
    static final String DB_HOST = "localhost";
    static final int    DB_PORT = 3306;
    static final String DB_NAME = "auctiondb";
    static final String DB_USER = "root";
    static final String DB_PASS = ""; // set a real password in production

    // Try MariaDB driver first, then MySQL driver. Build a URL that disables SSL.
    static Connection getConnection() throws Exception {
        String mariadbUrl = "jdbc:mariadb://"+DB_HOST+":"+DB_PORT+"/"+DB_NAME+"?useSSL=false";
        String mysqlUrl   = "jdbc:mysql://"+DB_HOST+":"+DB_PORT+"/"+DB_NAME+
                "?useSSL=false&requireSSL=false&allowPublicKeyRetrieval=true&serverTimezone=UTC";
        try {
            Class.forName("org.mariadb.jdbc.Driver");
            return DriverManager.getConnection(mariadbUrl, DB_USER, DB_PASS);
        } catch (ClassNotFoundException ignore) {
            Class.forName("com.mysql.cj.jdbc.Driver");
            return DriverManager.getConnection(mysqlUrl, DB_USER, DB_PASS);
        }
    }

    // --- Small helpers ---
    static String html(String s){
        if(s==null) return "";
        StringBuilder o=new StringBuilder(s.length());
        for(char c: s.toCharArray()){
            switch(c){
                case '&': o.append("&amp;"); break;
                case '<': o.append("&lt;");  break;
                case '>': o.append("&gt;");  break;
                case '"': o.append("&quot;"); break;
                case '\'':o.append("&#39;"); break;
                default:   o.append(c);
            }
        }
        return o.toString();
    }

    static Map<String,String> parseKV(String qs){
        Map<String,String> m=new LinkedHashMap<>();
        if(qs==null||qs.isEmpty()) return m;
        for(String part: qs.split("&")){
            if(part.isEmpty()) continue;
            String[] kv=part.split("=",2);
            String k=URLDecoder.decode(kv[0], StandardCharsets.UTF_8);
            String v=kv.length>1?URLDecoder.decode(kv[1], StandardCharsets.UTF_8):"";
            m.put(k,v);
        }
        return m;
    }

    static String readBody() throws IOException{
        String cl = System.getenv("CONTENT_LENGTH");
        int n = (cl==null||cl.isEmpty())?0:Integer.parseInt(cl);
        if(n<=0) return "";
        byte[] buf=new byte[n];
        int off=0; InputStream in=System.in;
        while(off<n){
            int r=in.read(buf, off, n-off);
            if(r<0) break; off+=r;
        }
        return new String(buf,0,off, StandardCharsets.UTF_8);
    }

    static String randHex(int nbytes){
        byte[] b=new byte[nbytes];
        new SecureRandom().nextBytes(b);
        StringBuilder sb=new StringBuilder(nbytes*2);
        for(byte x: b) sb.append(String.format("%02x", x));
        return sb.toString();
    }

    static void printHeader(String extra){
        System.out.print("Content-Type: text/html\r\n");
        if(extra!=null && !extra.isEmpty()) System.out.print(extra);
        System.out.print("\r\n");
    }

    static void top(String title){
        System.out.println("<!doctype html><html><head><meta charset='utf-8'><title="+html(title)+
                "</title><style>body{font-family:sans-serif;max-width:900px;margin:24px auto;padding:0 12px}form{margin:16px 0}input,button,select,textarea{padding:8px;margin:4px 0}a.button,button{border:1px solid #ccc;border-radius:8px;padding:8px 12px;text-decoration:none;display:inline-block}</style></head><body><h1>"+html(title)+"</h1>");
    }

    static void bottom(){ System.out.println("</body></html>"); }

    static void forms(String msg){
        if(msg!=null && !msg.isEmpty()) System.out.println("<p style='color:#b00'><strong>"+html(msg)+"</strong></p>");
        System.out.println("<h2>Log in</h2><form method='POST'><input type='hidden' name='action' value='login'>"+
                "Email: <input type='email' name='email' required><br>"+
                "Password: <input type='password' name='password' required><br>"+
                "<button type='submit'>Log in</button></form>");
        System.out.println("<h2>Register</h2><form method='POST'><input type='hidden' name='action' value='register'>"+
                "Email: <input type='email' name='email' required><br>"+
                "Password: <input type='password' name='password' required><br>"+
                "<button type='submit'>Create account</button></form>");
        System.out.println("<hr><p><a class='button' href='/cgi-bin/listings.cgi'>All open auctions</a> · "+
                "<a class='button' href='/cgi-bin/bid_sell.cgi'>Bid / Sell</a> · "+
                "<a class='button' href='/cgi-bin/transactions.cgi'>Your transactions</a></p>");
    }

    static void handlePost() throws Exception{
        String body = readBody();
        Map<String,String> kv = parseKV(body);
        String action = kv.getOrDefault("action", "");
        String email  = kv.getOrDefault("email",  "");
        String pass   = kv.getOrDefault("password"," ").trim();

        if(email.isEmpty() || pass.isEmpty() || !("register".equals(action) || "login".equals(action))){
            printHeader(""); top("Auction Portal — Auth"); forms("Please fill in all fields."); bottom(); return;
        }

        try(Connection cx = getConnection()){
            if(cx==null){ printHeader(""); top("Auction Portal — Auth"); forms("DB connection failed."); bottom(); return; }

            if("register".equals(action)){
                try(PreparedStatement ps=cx.prepareStatement("SELECT user_id FROM users WHERE email=? LIMIT 1")){
                    ps.setString(1, email);
                    try(ResultSet rs=ps.executeQuery()){
                        if(rs.next()){
                            printHeader(""); top("Auction Portal — Auth"); forms("Email already registered."); bottom(); return;
                        }
                    }
                }
                String salt = randHex(16);
                try(PreparedStatement ps=cx.prepareStatement(
                        "INSERT INTO users(email,password_salt,password_hash) VALUES(?, ?, SHA2(CONCAT(?, ?),256))",
                        Statement.RETURN_GENERATED_KEYS)){
                    ps.setString(1, email);
                    ps.setString(2, salt);
                    ps.setString(3, salt);
                    ps.setString(4, pass);
                    ps.executeUpdate();
                    long uid=0; try(ResultSet gk=ps.getGeneratedKeys()){ if(gk.next()) uid=gk.getLong(1); }
                    String tok = randHex(32);
                    try(PreparedStatement ps2=cx.prepareStatement(
                            "INSERT INTO sessions(session_id,user_id,expires_at) VALUES(?, ?, DATE_ADD(NOW(), INTERVAL 7 DAY))")){
                        ps2.setString(1, tok); ps2.setLong(2, uid); ps2.executeUpdate();
                    }
                    printHeader("Set-Cookie: SESSION_ID="+tok+"; Path=/; HttpOnly; SameSite=Lax\r\n");
                    top("Auction Portal — Auth");
                    System.out.println("<p>Registered! <a href='/cgi-bin/listings.cgi'>Continue to listings</a></p>");
                    bottom();
                    return;
                }
            }

            if("login".equals(action)){
                long uid=-1;
                try(PreparedStatement ps=cx.prepareStatement(
                        "SELECT user_id FROM users WHERE email=? AND password_hash = SHA2(CONCAT(password_salt,?),256) LIMIT 1")){
                    ps.setString(1, email); ps.setString(2, pass);
                    try(ResultSet rs=ps.executeQuery()){ if(rs.next()) uid=rs.getLong(1); }
                }
                if(uid<=0){ printHeader(""); top("Auction Portal — Auth"); forms("Invalid credentials."); bottom(); return; }
                String tok = randHex(32);
                try(PreparedStatement ps2=cx.prepareStatement(
                        "INSERT INTO sessions(session_id,user_id,expires_at) VALUES(?, ?, DATE_ADD(NOW(), INTERVAL 7 DAY))")){
                    ps2.setString(1, tok); ps2.setLong(2, uid); ps2.executeUpdate();
                }
                printHeader("Set-Cookie: SESSION_ID="+tok+"; Path=/; HttpOnly; SameSite=Lax\r\n");
                top("Auction Portal — Auth");
                System.out.println("<p>Logged in! <a href='/cgi-bin/listings.cgi'>Continue to listings</a></p>");
                bottom();
                return;
            }
        }

        printHeader(""); top("Auction Portal — Auth"); forms("Unknown action."); bottom();
    }

    public static void main(String[] args) throws Exception {
        String method = System.getenv("REQUEST_METHOD");
        if(method==null || method.equalsIgnoreCase("GET")){
            printHeader(""); top("Auction Portal — Auth"); forms(""); bottom();
        } else {
            handlePost();
        }
    }
}
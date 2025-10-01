#include <iostream>
#include <fstream>
#include <string>


int til() {
    // Output the HTTP header
    std::cout << "Content-Type: text/html\r\n\r\n";

    //printf ("%s\n\n", getenv("QUERY_STRING"))



    printf ("<!DOCTYPE html>\n");
    printf ("<html lang=\"en\">\n");
    printf ("  <head>\n\");\n");
    printf ("    <meta charset=\"UTF-8\">\n");
    printf ("    <meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">\n");
    printf ("    <!--<link rel=\"stylesheet\" type=\"text/css\" href=\"style/index.css\"> -->\n");
    printf ("    <title></title>\n");
    printf ("  </head>\n");
    printf ("  <body>\n");
    printf ("    <div class=\"container\">\n");
    printf ("    <div class=\"logo\">AM</div>\n");
    printf ("      <h1>Auction Market</h1>\n");
    printf ("      <p class=\"subtitle\">Place Made simple</p>\n");
    printf ("      <div class=\"features\">\n");
    printf ("        <div class=\"feature\">\n");
    printf ("          <h3>Place Bids</h3>\n");
    printf ("            <p>Find your next treasure or Start collecting today</p>\n");
    printf ("        </div>\n");
    printf ("        <div class=\"feature\">\n");
    printf ("          <h3>List Items</h3>\n");
    printf ("          <p>Trun your items into cash and Reach buyers worldwide!.</p>\n");
    printf ("        </div>\n");
    printf ("      </div>\n");
    printf ("      <button class=\"cta-button\" onclick=\"handleGetStarted()\">Get Started</button>\n");
    printf ("    </div>\n");
    printf ("    <script src=\"script/index.js\"></script>\n");
    printf ("  </body>\n");
    printf ("</html>\n");
    return 0;
}

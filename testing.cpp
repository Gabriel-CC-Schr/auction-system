#include <iostream>
#include <fstream>
#include <string>
#include <windows.h>

int main() {

    ShellExecuteA(NULL, "open", "chrome.exe", "http://localhost/cgi-bin/testing.cgi", // Adjust to your actual CGI path
            NULL,
            SW_SHOWNORMAL
        );


    // Output the HTTP header
    std::cout << "Content-Type: text/html\r\n\r\n";

    // Open the HTML file
    std::ifstream htmlFile("index.html"); // Replace with your HTML file path
    if (!htmlFile.is_open()) {
        std::cerr << "Error: Could not open the HTML file." << std::endl;
        return 1;
    }

    // Read and output the file content
    std::string line;
    while (std::getline(htmlFile, line)) {
        std::cout << line << std::endl;
    }

    htmlFile.close();



    return 0;
}

//*******************************************
// PROGRAMMING ASSIGNMENT 1 filename: style.cpp
// All-In Dragons Team
// Guillermo Rojas, Benjamin Ezike, Gabriel Cruz-Chavez, Paul Porter
//*******************************************

#include <iostream>
using namespace std;

int main() {
    cout << "Content-type: text/css\r\n\r\n";
    
    cout << "* { margin: 0; padding: 0; box-sizing: border-box; }\n";
    cout << "body { font-family: Arial, sans-serif; background: #f4f4f4; padding: 20px; }\n";
    
    cout << ".container { max-width: 600px; margin: 50px auto; background: white; padding: 30px; border-radius: 8px; box-shadow: 0 2px 10px rgba(0,0,0,0.1); }\n";
    
    cout << "h1 { color: #333; margin-bottom: 20px; text-align: center; }\n";
    cout << "h2 { color: #555; margin: 30px 0 15px 0; border-bottom: 2px solid #007bff; padding-bottom: 5px; }\n";
    
    cout << "form { margin: 20px 0; }\n";
    cout << "label { display: block; margin-bottom: 5px; color: #555; font-weight: bold; }\n";
    cout << "input[type=text], input[type=email], input[type=password] { width: 100%; padding: 10px; margin-bottom: 15px; border: 1px solid #ddd; border-radius: 4px; font-size: 14px; }\n";
    
    cout << "button, input[type=submit] { background: #007bff; color: white; padding: 12px 24px; border: none; border-radius: 4px; cursor: pointer; font-size: 16px; width: 100%; }\n";
    
    cout << ".nav { margin: 20px 0; padding: 15px; background: #e9ecef; border-radius: 4px; }\n";
    cout << ".nav a { color: #007bff; text-decoration: none; margin-right: 15px; }\n";
    
    cout << ".error { color: #dc3545; background: #f8d7da; padding: 10px; border-radius: 4px; margin: 15px 0; border: 1px solid #f5c6cb; }\n";
    cout << ".success { color: #155724; background: #d4edda; padding: 10px; border-radius: 4px; margin: 15px 0; border: 1px solid #c3e6cb; }\n";
    
    cout << "table { width: 100%; border-collapse: collapse; margin: 20px 0; }\n";
    cout << "th, td { padding: 12px; text-align: left; border-bottom: 1px solid #ddd; }\n";
    cout << "th { background: #007bff; color: white; }\n";
    
    cout << ".source-box { background: #f8f9fa; border: 1px solid #dee2e6; padding: 15px; margin: 20px 0; border-radius: 4px; overflow-x: auto; }\n";
    cout << ".source-box pre { font-family: 'Courier New', monospace; font-size: 12px; }\n";
    cout << ".source-box a { color: #007bff; text-decoration: none; }\n";
    
    cout << ".status { position: fixed; top: 0; left: 0; font-style: italic; padding: 6px; background: rgba(255,255,255,0.9); border-bottom-right-radius: 4px; box-shadow: 1px 1px 3px rgba(0,0,0,0.1); z-index: 1000; }\n";
    
    cout << ".logout-btn { margin-left: 8px; padding: 4px 12px; font-size: 12px; width: auto; display: inline-block; }\n";
    
    return 0;
}
#include "ollama.hpp"
#include <iostream>
#include <string>
using namespace std;

string page_break = "\n- - - - - - - - - - - - - -\n";

int main() {
  Ollama my_server("http://localhost:11434");
  
  string user_in;
  ollama::response last_response = my_server.generate("llama3.2", "How are you?");
  cout << last_response << page_break << endl;
  last_response = my_server.generate("llama3.2", "How old are you?", last_response);
  cout << last_response << page_break << endl;
  last_response = my_server.generate("llama3.2", "What was the first message I sent?", last_response);
  cout << last_response << page_break << endl;
}

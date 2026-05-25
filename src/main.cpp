#include <iostream>
#include <ranges>
#include <sstream>
#include <string>
#include <unistd.h>

int main() {
  std::cout << std::unitbuf;
  std::cerr << std::unitbuf;
  std::string line;
  std::string command;

  while (true) {
    std::cout << "$ ";
    std::getline(std::cin, line);
    std::stringstream ss(line);
    ss >> command;
    if (command == "echo") {
      std::string word;
      while (ss >> word)
        std::cout << word << " ";
      std::cout << std::endl;
    } else if (command == "exit")
      break;
    else if (command == "type") {
      bool found = false;
      std::string builtin[3] = {"echo", "exit", "type"};
      std::string command_to_know;
      ss >> command_to_know;
      for (int i = 0; i <= builtin->length(); i++)
        if (builtin[i] == command_to_know) {
          std::cout << command_to_know << " is a shell builtin\n";
          found = true;
        }
      if (!found) {
        std::string path_env = std::getenv("PATH");
        std::stringstream ss_path(path_env);
        std::string path;
        while (std::getline(ss_path, path, ':')) {
          std::string full_path = path + '/' + command_to_know;
          if (access(full_path.c_str(), X_OK) == 0) {
            std::cout << command_to_know << " is " << full_path << std::endl;
            found = true;
            break;
          }
        }
      }
      if (!found) {
        std::cout << command_to_know << ": not found\n";
      }
    } else
      std::cout << command << ": command not found\n";
  }
}
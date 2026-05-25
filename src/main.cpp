#include <iostream>
#include <string>

int main() {
  // Flush after every std::cout / std:cerr
  std::cout << std::unitbuf;
  std::cerr << std::unitbuf;

  while (true) {
    std::cout << "$ ";
    std::string input;
    std::getline(std::cin, input);

    std::string command = input.substr(0, input.find(' '));
    std::string parameters;

    const std::size_t parameterIndex = input.find(' ') + 1;
    if (parameterIndex != std::string::npos)
      parameters = input.substr(parameterIndex);

    if (command == "exit")
      break;

    if (command == "echo") {
      std::cout << parameters << std::endl;
    } else if (command == "type") {
      if (parameters == "echo" || parameters == "exit" || parameters == "type")
        std::cout << parameters << " is a shell builtin" << std::endl;
      else
        std::cout << parameters << ": not found" << std::endl;
    } else {
      std::cout << command << ": command not found" << std::endl;
    }
  }
}
#include <iostream>
#include <ranges>
#include <sstream>
#include <string>
#include <unistd.h>
#include <sys/wait.h>
#include <filesystem>
#include <regex>
#include <vector>

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
      std::vector<std::string> tokens;
      std::string current;
      bool in_single_quote = false;
      for (char c : line) {
        if (c == '\'') {
          in_single_quote = !in_single_quote;
        }
        else if (std::isspace(c) && !in_single_quote) {
          if (!current.empty()) {
            tokens.push_back(current);
            current.clear();
          }
        }
        else {
          current += c;
        }
      }
      if (!current.empty()) {
        tokens.push_back(current);
      }
      for(auto i:tokens)
        std::cout << i << " ";
      std::cout << std::endl;

    } else if (command == "exit") {

      break;

    }else if(command=="pwd"){
      std::cout << std::filesystem::current_path().string() << '\n';
    }else if (command == "cd") {
      std::string p = line.substr(3);
      p = std::regex_replace(p, std::regex("~"), std::getenv("HOME"));
      if (chdir(p.c_str()) != 0) {
        std::cout << "cd: " << p << ": No such file or directory\n";
      }
    } else if (command == "type") {

      bool found = false;
      std::string builtin[5] = {"echo", "exit", "type","pwd","cd"};
      std::string command_to_know;
      ss >> command_to_know;

      for (int i = 0; i < 5; i++) {
        if (builtin[i] == command_to_know) {
          std::cout << command_to_know << " is a shell builtin\n";
          found = true;
        }
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

    } else {

      std::vector<std::string> args;
      args.push_back(command);

      std::string arg;
      while (ss >> arg)
        args.push_back(arg);

      std::vector<char*> c_args;
      for (auto& s : args)
        c_args.push_back(&s[0]);

      c_args.push_back(nullptr);

      pid_t pid = fork();

      if (pid == 0) {

        execvp(command.c_str(), c_args.data());

        std::cout << command << ": command not found\n";
        exit(1);

      } else {

        waitpid(pid, nullptr, 0);
      }
    }
  }
}
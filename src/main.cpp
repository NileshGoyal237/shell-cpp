#include <iostream>
#include <ranges>
#include <sstream>
#include <string>
#include <unistd.h>
#include <sys/wait.h>
#include <filesystem>
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
      std::string word;
      while (ss >> word)
        std::cout << word << " ";
      std::cout << std::endl;

    } else if (command == "exit") {

      break;

    }else if(command=="pwd"){
      std::cout << std::filesystem::current_path().string() << '\n';
    } else if (command == "type") {

      bool found = false;
      std::string builtin[4] = {"echo", "exit", "type","pwd"};
      std::string command_to_know;
      ss >> command_to_know;

      for (int i = 0; i < 4; i++) {
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
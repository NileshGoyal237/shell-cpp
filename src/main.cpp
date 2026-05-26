#include <iostream>
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

    std::vector<std::string> tokens;

    std::string current;
    bool in_single_quote = false;
    std::char cc='!';
    for (char c : line) {

      if ((c == '\'' || c=='\"')) {
        if(c==cc)cc='!';
        else if(cc=='!')cc=c;
      }

      else if (std::isspace(c) && cc=='!') {

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

    if (tokens.empty())
      continue;

    command = tokens[0];

    if (command == "echo") {

      for (int i = 1; i < tokens.size(); i++) {
        std::cout << tokens[i];

        if (i + 1 < tokens.size())
          std::cout << " ";
      }

      std::cout << '\n';
    }

    else if (command == "exit") {

      break;
    }

    else if (command == "pwd") {

      std::cout << std::filesystem::current_path().string() << '\n';
    }

    else if (command == "cd") {

      std::string p;

      if (tokens.size() > 1)
        p = tokens[1];

      p = std::regex_replace(p, std::regex("~"), std::getenv("HOME"));

      if (chdir(p.c_str()) != 0) {
        std::cout << "cd: " << p << ": No such file or directory\n";
      }
    }

    else if (command == "type") {

      bool found = false;

      std::string builtin[5] = {
        "echo", "exit", "type", "pwd", "cd"
      };

      if (tokens.size() > 1) {

        std::string command_to_know = tokens[1];

        for (int i = 0; i < 5; i++) {

          if (builtin[i] == command_to_know) {

            std::cout << command_to_know
                      << " is a shell builtin\n";

            found = true;
          }
        }

        if (!found) {

          std::string path_env = std::getenv("PATH");
          std::stringstream ss_path(path_env);

          std::string path;

          while (std::getline(ss_path, path, ':')) {

            std::string full_path =
              path + '/' + command_to_know;

            if (access(full_path.c_str(), X_OK) == 0) {

              std::cout << command_to_know
                        << " is "
                        << full_path
                        << '\n';

              found = true;
              break;
            }
          }
        }

        if (!found) {

          std::cout << command_to_know
                    << ": not found\n";
        }
      }
    }

    else {

      std::vector<char*> c_args;

      for (auto& s : tokens)
        c_args.push_back(&s[0]);

      c_args.push_back(nullptr);

      pid_t pid = fork();

      if (pid == 0) {

        execvp(command.c_str(), c_args.data());

        std::cout << command
                  << ": command not found\n";

        exit(1);
      }

      else {

        waitpid(pid, nullptr, 0);
      }
    }
  }
}
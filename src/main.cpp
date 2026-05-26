#include <iostream>
#include <string>
#include <unistd.h>
#include <sys/wait.h>
#include <filesystem>
#include <regex>
#include <vector>
#include <fcntl.h>
#include <sys/stat.h>
#include <dirent.h>
#include <map>
#include <set>
#include <sstream>
#include <algorithm>
#include <fcntl.h>
#include <sys/stat.h>
#include <sstream>
#include <readline/readline.h>
#include <readline/history.h>
#include <cstring>

std::map<std::string,std::string> comp;

char* command_generator(const char* text, int state) {

    static int index;

    std::vector<std::string> commands = {
        "echo",
        "exit"
    };
    std::string path_env = std::getenv("PATH");

    std::stringstream ss(path_env);

    std::string path;

    while (std::getline(ss, path, ':')) {

        DIR* dir = opendir(path.c_str());

        if (!dir)
            continue;

        struct dirent* entry;

        while ((entry = readdir(dir)) != nullptr) {

            std::string file = entry->d_name;

            std::string full_path =
                path + "/" + file;

            if (access(full_path.c_str(), X_OK) == 0) {
                commands.push_back(file);
            }
        }

        closedir(dir);
    }

    if (state == 0)
        index = 0;

    while (index < commands.size()) {

        std::string cmd = commands[index++];

        if (cmd.substr(0, strlen(text)) == text)
            return strdup(cmd.c_str());
    }

    return nullptr;
}

char** command_completion(
    const char* text,
    int start,
    int end
) {

    if (start == 0) {

        rl_completion_append_character = ' ';

        return rl_completion_matches(
            text,
            command_generator
        );
    }

    std::string line = rl_line_buffer;

    std::stringstream ss(line);

    std::string cmd;

    ss >> cmd;

    if (comp.find(cmd) == comp.end())
        return nullptr;

    std::string current_word = text;

    std::stringstream line_ss(line);

    std::vector<std::string> words;

    std::string temp;

    while (line_ss >> temp)
        words.push_back(temp);

    std::string previous_word = "";

    if (words.size() >= 2)
        previous_word = words[words.size() - 2];

    std::string script_command =
        comp[cmd]
        + " "
        + cmd
        + " "
        + current_word
        + " "
        + "\"" + previous_word + "\"";
    
    std::string comp_line = line;

    std::string comp_point =
    std::to_string(line.length());

    char* old_line = getenv("COMP_LINE");

    char* old_point = getenv("COMP_POINT");

    setenv(
        "COMP_LINE",
        comp_line.c_str(),
        1
    );

    setenv(
        "COMP_POINT",
        comp_point.c_str(),
        1
    );
    FILE* fp = popen(script_command.c_str(), "r");

    if (!fp)
        return nullptr;

    std::vector<std::string> candidates;

    char buffer[1024];

    while (fgets(buffer, sizeof(buffer), fp)) {

        std::string candidate = buffer;

        if (!candidate.empty() &&
            candidate.back() == '\n')
            candidate.pop_back();

        candidates.push_back(candidate);
    }

    pclose(fp);

    if (candidates.empty())
        return nullptr;

    std::sort(
        candidates.begin(),
        candidates.end()
    );

    rl_completion_append_character = ' ';

    char** matches =
        (char**)malloc(
            (candidates.size() + 1)
            * sizeof(char*)
        );

    for (int i = 0; i < candidates.size(); i++) {

        matches[i] =
            strdup(candidates[i].c_str());
    }

    matches[candidates.size()] = nullptr;

    return matches;
}

int main() {
  std::cout << std::unitbuf;
  std::cerr << std::unitbuf;

  std::string line;
  std::string command;
  rl_attempted_completion_function =
    command_completion;
  
  rl_completion_append_character = ' ';
  while (true) {

    char* input = readline("$ ");

    if (!input)
        break;

    line = input;

    free(input);

    std::vector<std::string> tokens;

    std::string current;
    bool escape = false;
    char cc='!';
    char lc='!';
    for (char c : line) {
      if (escape) {
          current += c;
          escape = false;
      }

      else if (c == '\\' && cc!='\'') {
          escape = true;
      }
      else if (c == '\'') {

        if (cc == '!') {
            cc = '\'';
        }
        else if (cc == '\'') {
            cc = '!';
        }
        else {
            current += c;
        }
      }

      else if (c == '\"') {

          if (cc == '!') {
              cc = '\"';
          }
          else if (cc == '\"') {
              cc = '!';
          }
          else {
              current += c;
          }
      }

      else if (std::isspace(c) && cc=='!') {

        if (!current.empty()) {
          tokens.push_back(current);
          current.clear();
        }
      }

      else{
        current += c;
      }
      //lc=c;
    }

    if (!current.empty()) {
      tokens.push_back(current);
    }

    if (tokens.empty())
      continue;
    
    bool stdout_redirect = false;
    bool stderr_redirect = false;
    bool stdout_append = false;
    bool stderr_append = false;

    std::string output_file;
    std::string error_file;

    std::vector<std::string> actual_tokens;

    for (int i = 0; i < tokens.size(); i++) {

        if (tokens[i] == ">" || tokens[i] == "1>") {

            stdout_redirect = true;

            if (i + 1 < tokens.size())
                output_file = tokens[i + 1];

            i++;
        }

        else if (tokens[i] == "2>") {

            stderr_redirect = true;

            if (i + 1 < tokens.size())
                error_file = tokens[i + 1];

            i++;
        }

        else if (tokens[i] == ">>" || tokens[i]=="1>>") {
            stdout_redirect=true;
            stdout_append = true;

            if (i + 1 < tokens.size())
                output_file = tokens[i + 1];

            i++;
        }

        else if (tokens[i] == "2>>") {
            stderr_redirect=true;
            stderr_append = true;

            if (i + 1 < tokens.size())
                error_file = tokens[i + 1];

            i++;
        }

        else {
            actual_tokens.push_back(tokens[i]);
        }
    }

    tokens = actual_tokens;

    command = tokens[0];

    if (command == "echo") {

        int saved_stdout = dup(STDOUT_FILENO);
        int saved_stderr = dup(STDERR_FILENO);

        if (stdout_redirect) {
            int flags = O_WRONLY | O_CREAT;

            if (stdout_append)
                flags |= O_APPEND;
            else
                flags |= O_TRUNC;
            int fd = open(
                output_file.c_str(),
                flags ,
                0644
            );

            dup2(fd, STDOUT_FILENO);
            close(fd);
        }

        if (stderr_redirect) {
            
          int flags = O_WRONLY | O_CREAT;

            if (stderr_append)
                flags |= O_APPEND;
            else
                flags |= O_TRUNC;
            int fd = open(
                error_file.c_str(),
                flags,
                0644
            );

            dup2(fd, STDERR_FILENO);
            close(fd);
        }

        for (int i = 1; i < tokens.size(); i++) {

            std::cout << tokens[i];

            if (i + 1 < tokens.size())
                std::cout << " ";
        }

        std::cout << '\n';

        std::cout.flush();
        std::cerr.flush();

        dup2(saved_stdout, STDOUT_FILENO);
        dup2(saved_stderr, STDERR_FILENO);

        close(saved_stdout);
        close(saved_stderr);
    }

    else if (command == "exit") {

      break;
    }
    else if(command=="complete"){
      if (tokens.size() >= 3 ) {
          if(tokens[1] == "-C"){
            comp[tokens[3]]=tokens[2];
          }
          else if(tokens[1]=="-p"){
            if(comp.find(tokens[2])==comp.end()){
              std::cout
                << "complete: "
                << tokens[2]
                << ": no completion specification\n";
            }
            else{
              std::cout
                << "complete -C '"
                << comp[tokens[2]]
                << "' "
                << tokens[2]
                << '\n';

            }
          }
      }
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

      std::string builtin[6] = {
        "echo", "exit", "type", "pwd", "cd" ,"complete"
      };

      if (tokens.size() > 1) {

        std::string command_to_know = tokens[1];

        for (int i = 0; i < 6; i++) {

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

          if (stdout_redirect) {
              int flags = O_WRONLY | O_CREAT;

              if (stdout_append)
                  flags |= O_APPEND;
              else
                  flags |= O_TRUNC;
              int fd = open(
                  output_file.c_str(),
                  flags,
                  0644
              );

              dup2(fd, STDOUT_FILENO);

              close(fd);
          }

          if (stderr_redirect) {
              int flags = O_WRONLY | O_CREAT;

              if (stderr_append)
                  flags |= O_APPEND;
              else
                  flags |= O_TRUNC;
              int fd = open(
                  error_file.c_str(),
                  flags,
                  0644
              );

              dup2(fd, STDERR_FILENO);

              close(fd);
          }

          execvp(command.c_str(), c_args.data());

          std::cerr << command
                    << ": command not found\n";

          exit(1);
      }

      else {

        waitpid(pid, nullptr, 0);
      }
    }
  }
}
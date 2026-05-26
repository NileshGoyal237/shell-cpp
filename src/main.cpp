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
#include <termios.h>

void setRawMode() {
    struct termios term;
    tcgetattr(STDIN_FILENO, &term);
    term.c_lflag &= ~(ICANON | ECHO);
    term.c_cc[VMIN] = 1;
    term.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSANOW, &term);
}

std::map<std::string,std::string> comp;
int job_count = 0;
int his_count=0;

struct Job {
    int number;
    pid_t pid;
    std::string command;
    std::string status;
};
struct History {
    int number;
    std::string command;
};

std::vector<Job> jobs;
std::vector<History> history;

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
        return rl_completion_matches(text, command_generator);
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
        comp[cmd] + " " + cmd + " " + current_word + " " + "\"" + previous_word + "\"";

    std::string comp_line = rl_line_buffer;
    std::string comp_point = std::to_string(rl_point);

    setenv("COMP_LINE", comp_line.c_str(), 1);
    setenv("COMP_POINT", comp_point.c_str(), 1);

    FILE* fp = popen(script_command.c_str(), "r");
    if (!fp)
        return nullptr;

    std::vector<std::string> candidates;
    char buffer[1024];
    while (fgets(buffer, sizeof(buffer), fp)) {
        std::string candidate = buffer;
        if (!candidate.empty() && candidate.back() == '\n')
            candidate.pop_back();
        if (!candidate.empty())
            candidates.push_back(candidate);
    }
    pclose(fp);

    if (candidates.empty())
        return nullptr;

    std::sort(candidates.begin(), candidates.end());

    rl_attempted_completion_over = 1;

    if (candidates.size() == 1) {
        rl_completion_append_character = ' ';
        
        char** matches = (char**)malloc(3 * sizeof(char*));
        matches[0] = strdup(candidates[0].c_str());  // <-- the completed word, not ""
        matches[1] = strdup(candidates[0].c_str());
        matches[2] = nullptr;
        return matches;
    }

    // Multiple candidates
    rl_completion_append_character = '\0';

    // Compute LCP
    std::string lcp = candidates[0];
    for (int i = 1; i < (int)candidates.size(); i++) {
        int j = 0;
        while (j < (int)lcp.size() && j < (int)candidates[i].size() && lcp[j] == candidates[i][j])
            j++;
        lcp = lcp.substr(0, j);
    }

    char** matches = (char**)malloc((candidates.size() + 2) * sizeof(char*));

    // If LCP is longer than what user typed, set it as matches[0] so readline inserts it
    if (lcp.size() > strlen(text)) {
        matches[0] = strdup(lcp.c_str());
    } else {
        matches[0] = strdup("");  // no new chars to add, just show list
    }

    for (int i = 0; i < (int)candidates.size(); i++) {
        matches[i + 1] = strdup(candidates[i].c_str());
    }
    matches[candidates.size() + 1] = nullptr;

    return matches;
}

int main() {
  std::cout << std::unitbuf;
  std::cerr << std::unitbuf;

  using_history();

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
    if (!line.empty())
      add_history(line.c_str()); 

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

    bool background = false;

    if (!tokens.empty() && tokens.back() == "&") {
        background = true;
        tokens.pop_back();
    }

    // Split tokens into commands at |
    std::vector<std::vector<std::string>> pipeline;
    std::vector<std::string> current_cmd;

    for (int i = 0; i < tokens.size(); i++) {
        if (tokens[i] == "|") {
            if (!current_cmd.empty()) {
                pipeline.push_back(current_cmd);
                current_cmd.clear();
            }
        } else {
            current_cmd.push_back(tokens[i]);
        }
    }
    if (!current_cmd.empty())
        pipeline.push_back(current_cmd);

    his_count++;
    History hiss;
    hiss.number=his_count;
    hiss.command=line;
    history.push_back(hiss);
    
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

    if (command == "echo" && pipeline.size()==1) {

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

    else if (command == "exit" && pipeline.size()==1) {

      break;
    }

    else if (command == "history" && pipeline.size()==1){
      if(tokens.size()>1){
        std::string numm=tokens[1];
        int check=0;
        for(auto i:numm)check=check*10+(i-'0');
        for(int i=history.size()-check;i<history.size();i++){
          std::cout << history[i].number << "  " << history[i].command << "\n";
        }
      }
      else{
        for(int i=0;i<history.size();i++){
          std::cout << history[i].number << "  " << history[i].command << "\n";
        }
      }
    }
    else if(command=="complete" && pipeline.size()==1){
      if (tokens.size() >= 3 ) {
          if(tokens[1]=="-r"){
            comp.erase(tokens[2]);
          }
          else if(tokens[1] == "-C"){
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
    else if (command == "pwd" && pipeline.size()==1) {

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
    else if(command=="jobs"){
      for(int i = 0; i < jobs.size(); i++){
        int status;
        pid_t result = waitpid(jobs[i].pid, &status, WNOHANG);
        if(result > 0){
            jobs[i].status = "Done";
        } else if(result == 0) {
            // Still check if process actually exists
            if(kill(jobs[i].pid, 0) != 0){
                jobs[i].status = "Done";
            }
        } else {
            // result == -1, process no longer exists
            jobs[i].status = "Done";
        }
      }
      for(int i=0;i<jobs.size();i++){
        if(i==jobs.size()-1)std::cout << '[' << jobs[i].number << "]+  " << jobs[i].status << "                 " << jobs[i].command << "\n"; 
        else if(i==jobs.size()-2)std::cout << '[' << jobs[i].number << "]-  " << jobs[i].status << "                 " << jobs[i].command << "\n";
        else std::cout << '[' << jobs[i].number << "]   " << jobs[i].status << "                 " << jobs[i].command << "\n";
      }
      jobs.erase(
          std::remove_if(jobs.begin(), jobs.end(), 
              [](const Job& j){ return j.status == "Done"; }),
          jobs.end()
      );
    }

    else if (command == "type" && pipeline.size()==1) {

      bool found = false;

      std::string builtin[8] = {
        "echo", "exit", "type", "pwd", "cd" ,"complete","jobs","history"
      };

      if (tokens.size() > 1) {

        std::string command_to_know = tokens[1];

        for (int i = 0; i < 8; i++) {

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
      if (pipeline.size() > 1) {
          int n = pipeline.size();
          std::vector<std::array<int, 2>> pipes(n - 1);
          for (int i = 0; i < n - 1; i++)
              pipe(pipes[i].data());

          std::vector<pid_t> pids;

          for (int i = 0; i < n; i++) {
              pid_t pid = fork();

              if (pid == 0) {
                  // Set up pipe redirections
                  if (i > 0)
                      dup2(pipes[i-1][0], STDIN_FILENO);
                  if (i < n - 1)
                      dup2(pipes[i][1], STDOUT_FILENO);

                  // Close all pipe fds
                  for (int j = 0; j < n - 1; j++) {
                      close(pipes[j][0]);
                      close(pipes[j][1]);
                  }

                  // Handle file redirections only for last command
                  if (i == n - 1) {
                      if (stdout_redirect) {
                          int flags = O_WRONLY | O_CREAT;
                          if (stdout_append) flags |= O_APPEND;
                          else flags |= O_TRUNC;
                          int fd = open(output_file.c_str(), flags, 0644);
                          dup2(fd, STDOUT_FILENO);
                          close(fd);
                      }
                      if (stderr_redirect) {
                          int flags = O_WRONLY | O_CREAT;
                          if (stderr_append) flags |= O_APPEND;
                          else flags |= O_TRUNC;
                          int fd = open(error_file.c_str(), flags, 0644);
                          dup2(fd, STDERR_FILENO);
                          close(fd);
                      }
                  }

                  std::string cmd = pipeline[i][0];

                  // Handle builtins
                  if (cmd == "echo") {
                      for (int j = 1; j < (int)pipeline[i].size(); j++) {
                          std::cout << pipeline[i][j];
                          if (j + 1 < (int)pipeline[i].size())
                              std::cout << " ";
                      }
                      std::cout << "\n";
                      exit(0);
                  }
                  else if (cmd == "pwd") {
                      std::cout << std::filesystem::current_path().string() << "\n";
                      exit(0);
                  }
                  else if (cmd == "type") {
                      if (pipeline[i].size() > 1) {
                          std::string command_to_know = pipeline[i][1];
                          std::string builtins[] = {"echo", "exit", "type", "pwd", "cd", "complete", "jobs"};
                          bool found = false;
                          for (auto& b : builtins) {
                              if (b == command_to_know) {
                                  std::cout << command_to_know << " is a shell builtin\n";
                                  found = true;
                                  break;
                              }
                          }
                          if (!found) {
                              std::string path_env = std::getenv("PATH");
                              std::stringstream ss_path(path_env);
                              std::string p;
                              while (std::getline(ss_path, p, ':')) {
                                  std::string full_path = p + '/' + command_to_know;
                                  if (access(full_path.c_str(), X_OK) == 0) {
                                      std::cout << command_to_know << " is " << full_path << "\n";
                                      found = true;
                                      break;
                                  }
                              }
                          }
                          if (!found)
                              std::cout << command_to_know << ": not found\n";
                      }
                      exit(0);
                  }

                  // Not a builtin — execvp
                  std::vector<char*> c_args;
                  for (auto& s : pipeline[i])
                      c_args.push_back(&s[0]);
                  c_args.push_back(nullptr);

                  execvp(cmd.c_str(), c_args.data());
                  std::cerr << cmd << ": command not found\n";
                  exit(1);
              }

              pids.push_back(pid);
          }

          // Parent closes all pipes
          for (int i = 0; i < n - 1; i++) {
              close(pipes[i][0]);
              close(pipes[i][1]);
          }

          // Wait for all children
          for (pid_t p : pids)
              waitpid(p, nullptr, 0);
      }

      else{std::vector<char*> c_args;

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
        if (background) {
            std::set<int> used;
            for (auto& j : jobs)
                used.insert(j.number);

            int next_num = 1;
            while (used.count(next_num))
                next_num++;
            std::string cmd = line;
            if(!cmd.empty() && cmd.back() == '&')
                cmd.pop_back();
            if(!cmd.empty() && cmd.back() == ' ')
                cmd.pop_back();
            Job j;
            j.number = next_num;
            j.pid = pid;
            j.command = cmd;  // original input line
            j.status = "Running";
            jobs.push_back(j);
            std::cout << "[" << next_num << "] " << pid << "\n";
            // don't waitpid — let it run in background
        } else{
          waitpid(pid, nullptr, 0);
        }
      }}
    }
    if(command!="jobs"){  
      for(int i = 0; i < jobs.size(); i++){
        int status;
        pid_t result = waitpid(jobs[i].pid, &status, WNOHANG);
        if(result > 0){
            jobs[i].status = "Done";
        } else if(result == 0) {
            // Still check if process actually exists
            if(kill(jobs[i].pid, 0) != 0){
                jobs[i].status = "Done";
            }
        } else {
            // result == -1, process no longer exists
            jobs[i].status = "Done";
        }
      }
      for(int i=0;i<jobs.size();i++){
        if(i==jobs.size()-1 && jobs[i].status == "Done")std::cout << '[' << jobs[i].number << "]+  " << jobs[i].status << "                 " << jobs[i].command << "\n"; 
        else if(i==jobs.size()-2 && jobs[i].status == "Done")std::cout << '[' << jobs[i].number << "]-  " << jobs[i].status << "                 " << jobs[i].command << "\n";
        else if(jobs[i].status == "Done") std::cout << '[' << jobs[i].number << "]   " << jobs[i].status << "                 " << jobs[i].command << "\n";
      }
      jobs.erase(
          std::remove_if(jobs.begin(), jobs.end(), 
              [](const Job& j){ return j.status == "Done"; }),
          jobs.end()
      );
    }
  }
}
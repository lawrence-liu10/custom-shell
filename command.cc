/*
 * CS252: Shell project
 *
 * Template file.
 * You will need to add more code here to execute the command table.
 *
 * NOTE: You are responsible for fixing any bugs this code may have!
 *
 * DO NOT PUT THIS PROJECT IN A PUBLIC REPOSITORY LIKE GIT. IF YOU WANT 
 * TO MAKE IT PUBLICALLY AVAILABLE YOU NEED TO REMOVE ANY SKELETON CODE 
 * AND REWRITE YOUR PROJECT SO IT IMPLEMENTS FUNCTIONALITY DIFFERENT THAN
 * WHAT IS SPECIFIED IN THE HANDOUT. WE OFTEN REUSE PART OF THE PROJECTS FROM  
 * SEMESTER TO SEMESTER AND PUTTING YOUR CODE IN A PUBLIC REPOSITORY
 * MAY FACILITATE ACADEMIC DISHONESTY.
 */

#include <cstdio>
#include <cstdlib>
#include <sys/types.h>
#include <unistd.h>
#include <wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <iostream>
#include <string.h>
#include <dirent.h>
#include <fnmatch.h>
#include <algorithm>
#include <vector>
#include <pwd.h>

#include "command.hh"
#include "shell.hh"

extern void yyrestart(FILE *);
extern int yyparse(void);
extern char ** environ;

static bool hasWild(const std::string &s) {
  return s.find_first_of("*?[") != std::string::npos;
}

static std::vector<std::string> expandWildcards(const std::string &pattern) {

  if (!hasWild(pattern)) {
    return { pattern };
  }

  std::vector<std::string> parts;
  size_t start = 0;
  size_t slash;
  while ((slash = pattern.find('/', start)) != std::string::npos) {
    parts.push_back(pattern.substr(start, slash - start));
    start = slash + 1;
  }
  parts.push_back(pattern.substr(start));

  std::vector<std::string> paths;
  if (!pattern.empty() && pattern[0] == '/') {
    paths.emplace_back("/");
  }
  else {
    paths.emplace_back("");
  }

  for (const std::string &comp : parts) {
    std::vector<std::string> next;

    for (const std::string &base : paths) {
      std::string dir = base.empty() ? "." : base;
      if (!dir.empty() && dir.back() != '/') {
        dir += '/';
      }

      if (!hasWild(comp)) {
        std::string candidate = (base.empty() ? "" :
                                (base.back() == '/' ? base : base + "/")) + comp;
        next.push_back(candidate);
        continue;
      }

      DIR *d = opendir(dir.c_str());
      if (!d) continue;

      struct dirent *ent;
      while ((ent = readdir(d)) != nullptr) {
        std::string name(ent->d_name);

        if (name[0] == '.' && comp[0] != '.') {
          continue;
        }

        if (fnmatch(comp.c_str(), name.c_str(), 0) == 0) {
          std::string match = (base.empty() ? "" :
                                (base.back() == '/' ? base : base + "/")) + name;
          next.push_back(match);
        }
      }
      closedir(d);
    }
    paths.swap(next);
    if (paths.empty()) break;
  }
  if (paths.empty()) {
    paths.push_back(pattern);
  }
  std::sort(paths.begin(), paths.end());
  return paths;
}

static std::string expandTilde(const std::string &arg) {
  if (arg.empty() || arg[0] != '~') {
    return arg;
  }

  size_t slash = arg.find('/');
  std::string user = arg.substr(1, (slash == std::string::npos ?
                                    std::string::npos : slash - 1));
  std::string rest = (slash == std::string::npos ? "" : arg.substr(slash));

  std::string home;

  if (user.empty()) {
    const char *h = getenv("HOME");
    if (h) home = h;
  }
  else {
    struct passwd *pw = getpwnam(user.c_str());
    if (pw) home = pw->pw_dir;
  }

  if (home.empty()) {
    return arg;
  }

  return home + rest;
}


Command::Command() {
    // Initialize a new vector of Simple Commands
    _simpleCommands = std::vector<SimpleCommand *>();

    _outFile = NULL;
    _inFile = NULL;
    _errFile = NULL;
    _background = false;
    _append = false;
    _inCount = 0;
    _outCount = 0;
}

void Command::insertSimpleCommand( SimpleCommand * simpleCommand ) {
    // add the simple command to the vector
    _simpleCommands.push_back(simpleCommand);
}

void Command::clear() {
    // deallocate all the simple commands in the command vector
    for (auto simpleCommand : _simpleCommands) {
        delete simpleCommand;
    }

    // remove all references to the simple commands we've deallocated
    // (basically just sets the size to 0)
    _simpleCommands.clear();

    if ( _outFile ) {
        delete _outFile;
    }
    _outFile = NULL;

    if ( _inFile ) {
        delete _inFile;
    }
    _inFile = NULL;

    if ( _errFile ) {
        delete _errFile;
    }
    _errFile = NULL;

    _background = false;

    _append = false;
    _inCount = 0;
    _outCount = 0;
    _errCount = 0;
}

void Command::print() {
    printf("\n\n");
    printf("              COMMAND TABLE                \n");
    printf("\n");
    printf("  #   Simple Commands\n");
    printf("  --- ----------------------------------------------------------\n");

    int i = 0;
    // iterate over the simple commands and print them nicely
    for ( auto & simpleCommand : _simpleCommands ) {
        printf("  %-3d ", i++ );
        simpleCommand->print();
    }

    printf( "\n\n" );
    printf( "  Output       Input        Error        Background\n" );
    printf( "  ------------ ------------ ------------ ------------\n" );
    printf( "  %-12s %-12s %-12s %-12s\n",
            _outFile?_outFile->c_str():"default",
            _inFile?_inFile->c_str():"default",
            _errFile?_errFile->c_str():"default",
            _background?"YES":"NO");
    printf( "\n\n" );
}

void Command::execute() {
    // Don't do anything if there are no simple commands
    if ( _simpleCommands.size() == 0 ) {
        Shell::prompt();
        return;
    }

    // check for multiple redirects
    if (_inCount > 1) {
      printf("Ambiguous input redirect.\n");
      clear();
      Shell::prompt();
      return;
    }
    else if (_outCount > 1) {
      printf("Ambiguous output redirect.\n");
      clear();
      Shell::prompt();
      return;
    }
    else if (_errCount > 1) {
      printf("Ambiguous error redirect.\n");
      clear();
      Shell::prompt();
      return;
    }

    // Print contents of Command data structure
    //print();

    // Add execution here
    // For every simple command fork a new process
    // Setup i/o redirection
    // and call exec

    // save default in, out, and err
    int defaultin  = dup( 0 );
    int defaultout = dup( 1 );
    int defaulterr = dup( 2 );

    // read file input, or use stdin
    int fdin;
    if (_inFile) {
        fdin = open(_inFile->c_str(), O_RDONLY);
    }
    else {
        fdin = dup(defaultin);
    }

    // create err file in RW if directing, and check if appending
    int fderr;
    if (_errFile) {
        if(_append) {
            fderr = open(_errFile->c_str(), O_WRONLY | O_CREAT | O_APPEND , 0600);
        }
        else {
            fderr = open(_errFile->c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0600);
        }
    }
    else {
        fderr = dup(defaulterr);
    }

    // move 2 to point to fderr for (potential) redirection
    dup2(fderr, 2);
    close(fderr);

    int prevPipeRead = fdin;
    std::vector<pid_t> childPids;

    for (unsigned long i = 0; i < _simpleCommands.size(); i++) {
      // setenv A = B
      if (strcmp(_simpleCommands[i]->_arguments[0]->c_str(), "setenv") == 0) {
      if (_simpleCommands[i]->_arguments.size() < 3) {
        fprintf(stderr, "setenv: missing arguments\n");
      }
      else {
        if (setenv(_simpleCommands[i]->_arguments[1]->c_str(),
                    _simpleCommands[i]->_arguments[2]->c_str(),
                    1) != 0) {
          perror("setenv");
        }
      }

      dup2(defaultin, 0);
      dup2(defaultout, 1);
      dup2(defaulterr, 2);
      close(defaultin);
      close(defaultout);
      close(defaulterr);
      close(fdin);

      clear();
      Shell::prompt();
      return;
      }
      else if (strcmp(_simpleCommands[i]->_arguments[0]->c_str(), "unsetenv") == 0) {
        if (_simpleCommands[i]->_arguments.size() < 2) {
          fprintf(stderr, "unsetenv: missing variable name\n");
        }
        else {
          if (unsetenv(_simpleCommands[i]->_arguments[1]->c_str()) != 0) {
            perror("unsetenv");
          }
        }

        dup2(defaultin, 0);
        dup2(defaultout, 1);
        dup2(defaulterr, 2);
        close(defaultin);
        close(defaultout);
        close(defaulterr);
        close(fdin);

        clear();
        Shell::prompt();
        return;
      }
      if (strcmp(_simpleCommands[i]->_arguments[0]->c_str(), "cd") == 0) {
        const char *dest = nullptr;

        if (_simpleCommands[i]->_arguments.size() == 1) {
          dest = getenv("HOME");
          if (!dest) {
            dest = "/";
          }
        }
        else {
          std::string arg = *(_simpleCommands[i]->_arguments[1]);

          if (arg[0] == '$') {
            std::string var;

            if (arg.size() > 3 && arg[1] == '{' && arg.back() == '}') {
              var = arg.substr(2, arg.size() - 3);
            }
            else {
              var = arg.substr(1);
            }
            const char *val = getenv(var.c_str());
            dest = val ? val : arg.c_str();
          }
          else {
            dest = arg.c_str();
          }
        }
        if (chdir(dest) != 0) {
          fprintf(stderr, "cd: can't cd to %s\n",
                  (_simpleCommands[i]->_arguments.size() == 1) ? dest :
                  _simpleCommands[i]->_arguments[1]->c_str());
        }

        // done processing. restore stdin/out/err and close fds
        dup2(defaultin, 0);
        dup2(defaultout, 1);
        dup2(defaulterr, 2);
        close(defaultin);
        close(defaultout);
        close(defaulterr);
        close(fdin);

        clear();
        Shell::prompt();
        return;
      }

      dup2(prevPipeRead, 0);
      close(prevPipeRead);

      // for the last simple command, check if we need to output (append) to a file
      if (i == _simpleCommands.size() - 1) {
          int fdout;
          if (_outFile) {
              if (_append) {
                  fdout = open(_outFile->c_str(), O_WRONLY | O_CREAT | O_APPEND , 0600);
              }
              else {
                  fdout = open(_outFile->c_str(), O_WRONLY | O_CREAT | O_TRUNC , 0600);
              }
          }
          else {
              fdout = dup(defaultout);
          }
          dup2(fdout, 1);
          close(fdout);
      }
      else {
          // keep piping to last command
          int pipefd[2];
          if (pipe(pipefd)) {
              perror("pipe");
              break;
          }
          dup2(pipefd[1], 1); // write back to the pipe
          close(pipefd[1]);
          prevPipeRead = pipefd[0]; // send pipe as input to next
      }

      //execute
      pid_t pid = fork();
      if (pid == 0) {

        // printenv happens in child process
        if (strcmp(_simpleCommands[i]->_arguments[0]->c_str(), "printenv") == 0) {
          for (int i = 0; environ[i]; i++) {
          printf("%s\n", environ[i]);
          }
          exit(0);

        }
        // TODO fix
        // source
        /*
        if (strcmp(_simpleCommands[i]->_arguments[0]->c_str(), "source") == 0) {
          if (_simpleCommands[i]->_arguments.size() < 2) {
            fprintf(stderr, "source: no filename");
          }
          else {
            const char *fname = _simpleCommands[i]->_arguments[1]->c_str();
            FILE *fp = fopen(fname, "r");
            if (!fp) perror("source");
            else {
              yyrestart(fp);
              yyparse();
              yyrestart(stdin);
              fclose(fp);
            }
          }
          clear();
          Shell::prompt();
          return;
        }
        */

        close(defaultin);
        close(defaultout);
        close(defaulterr);

        // read the args and execute
        std::vector<const char *> args;
        for (size_t j = 0; j < _simpleCommands[i]->_arguments.size(); j++) {
          // check first for tilde/wildcards, expand, and push back
          std::string tildeStr = expandTilde(*_simpleCommands[i]->_arguments[j]);
          std::vector<std::string> exp = expandWildcards(tildeStr);
          for (const std::string &s : exp) {
            args.push_back(strdup(s.c_str()));
          }
        }
        args.push_back(nullptr);

        execvp(args[0], const_cast<char* const*>(args.data()));
        exit(1);
      }
      else if (pid < 0) { // fork error
          break;
      }

      childPids.push_back(pid);
    }
    // done executing, restore stdin/out/err and close fds
    dup2(defaultin, 0);
    dup2(defaultout, 1);
    dup2(defaulterr, 2);
    close(defaultin);
    close(defaultout);
    close(defaulterr);

    // receive SIGCHLD command and set status for {?} env var
    if (!_background) {
        for (pid_t cpid : childPids) {
          int st;
          waitpid(cpid, &st, 0);
          g_last_status = (WIFEXITED(st) ? WEXITSTATUS(st) : 1);
        }
    }
    // {!} env var
    if (_background) {
      g_last_bgpid = childPids.back();
    }

    // {_} env var
    if (!_simpleCommands.empty() && !_simpleCommands.back()->_arguments.empty()) {
      g_last_arg = *(_simpleCommands.back()->_arguments.back());
    }

    // Clear to prepare for next command
    clear();

    // Print new prompt (only when being used by a person)
    Shell::prompt();
}

SimpleCommand * Command::_currentSimpleCommand;

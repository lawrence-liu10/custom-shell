
/*
 *
 * CS-252 
 * shell.l: lexical analyzer for shell
 * You have to extend it.
 *
 */

%{

#include <cstring>
#include <string>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include "y.tab.hh"
#include "shell.hh"
#include <cstdlib>

extern "C" char * read_line();

int mygetc(FILE * f) {
  static char *p;
  char ch;

  if (!isatty(0)) {
  // stdin is not a tty. Call real getc
    return getc(f);
  }

  // stdin is a tty. Call our read_line.
  if (p==NULL || *p == 0) {
    char * s = read_line();
    p = s;
  }

  ch = *p;
  p++;

  return ch;
}

#undef getc
#define getc(f) mygetc(f)

static  void yyunput (int c,char *buf_ptr  );

void myunputc(int c) {
  unput(c);
}

// run a subshell command and save output
static std::string executeFunc(std::string &subshellCmd) {
  int to_c[2], from_c[2];
  pipe(to_c);
  pipe(from_c);

  pid_t pid = fork();
  if (pid == 0) { // child, redirect to pipes and execute
    dup2(to_c[0], 0);
    dup2(from_c[1], 1);

    close(to_c[1]);
    close(to_c[0]);
    execl("/proc/self/exe", "/proc/self/exe", (char *)NULL);
    _exit(1);
  }
  // close hanging fds
  close(to_c[0]);
  close(from_c[1]);

  // write to child's stdin
  write(to_c[1], subshellCmd.c_str(), subshellCmd.size());
  write(to_c[1], "\n", 1);
  close(to_c[1]);

  // read the child output
  std::string out;
  char buf[512];
  ssize_t n;
  while ( (n = read(from_c[0], buf, sizeof buf)) > 0) {
    out.append(buf, n);
  }
  close(from_c[0]);

  // receive SIGCHLD
  int status;
  waitpid(pid, &status, 0);

  // make newlines spaces for inline substitution
  for (char &c : out) {
    if (c == '\n') {
      c = ' ';
    }
  }

  // trim trailing whitespaces
  while (!out.empty() && (out.back() == ' ' || out.back() == '\t')) {
    out.pop_back();
  }
  return out;

}

// push s' chars back in reverse order
static void push_back(const std::string &s) {
  for (auto it = s.rbegin(); it != s.rend(); ++it) {
    yyunput(*it, yytext);
  }
}

// expand env and shell variables
static std::string expand_var(const std::string &key) {
  if (key == "$") return std::to_string(getpid());
  if (key == "?") return std::to_string(g_last_status);
  if (key == "!") return std::to_string(g_last_bgpid);
  if (key == "_") return g_last_arg;
  if (key == "SHELL") return g_shell_path;
  const char *val = getenv(key.c_str());
  return val ? val : std::string();
}


%}

%option noyywrap
%x SQUOTE DQUOTE

%%

\n {
  return NEWLINE;
}

[ \t] {
  /* Discard spaces andtabs */
}

">" {
  return GREAT;
}

"|" {
  return PIPE;
}

"<" {
  return LESS;
}

">&" {
  return GREATAMPERSAND;
}

">>" {
  return GREATGREAT;
}

">>&" {
  return GREATGREATAMPERSAND;
}

"2>" {
  return TWOGREAT;
}

"&" {
  return AMPERSAND;
}

";" {
  return NEWLINE;
}


\"([^\\\"\n]|\\.)*\" {
  // escapes inside of quotes
  std::string tmp(yytext + 1, yyleng - 2);
  std::string cooked;
  cooked.reserve(tmp.size());

  // parse for escapes
  for (size_t i = 0; i < tmp.size(); ++i) {
    if (tmp[i] == '\\' && i + 1 < tmp.size()) {
      cooked.push_back(tmp[++i]);
    }
    else {
      cooked.push_back(tmp[i]);
    }
  }
  yylval.cpp_string = new std::string(cooked);
  return WORD;

}

\$\{[A-Za-z0-9_!?\$]+\} {
  // variable expansion ${var}
  std::string name(yytext + 2, yyleng - 3);
  std::string val = expand_var(name);

  for (auto it = val.rbegin(); it != val.rend(); ++it) {
    yyunput(*it, yytext);
  }
}

\$\([^()\n]*\) {
  // subshell $()
  std::string inner(yytext + 2, yyleng - 3);
  push_back(executeFunc(inner));
}

(\\.|[^ \t\n|><&;\\\$\{\}])+ {
  // regular WORDS/escaped chars
  std::string raw(yytext);
  std::string cooked;

  cooked.reserve(raw.size());
  bool esc = false;

  // parse the word, substitute with escape if necessary
  for (char ch : raw) {
    if (esc) {
      cooked.push_back(ch);
      esc = false;
    }
    else if (ch == '\\') {
      esc = true;
    }
    else {
      cooked.push_back(ch);
    }
  }
  yylval.cpp_string = new std::string(cooked);
  return WORD;
}


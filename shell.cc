#include <cstdio>
#include <unistd.h>
#include <limits.h>

#include "shell.hh"
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>



int yyparse(void);
void yyrestart(FILE *file);

int g_last_status = 0;
pid_t g_last_bgpid = 0;
std::string g_last_arg = "";
std::string g_shell_path;

void Shell::prompt() {
  if (isatty(0)) {
    printf("myshell>");
  }
  fflush(stdout);
}

extern "C" void ctr_handler (int sig) {
  printf("\n");
  Shell::prompt();
}

// child process termination signal handler
extern "C" void zombie_handler(int /*sig*/) {
  wait3(0, 0, NULL);
  while (waitpid(-1, NULL, WNOHANG) > 0);
}

int main(int argc, char **argv) {
  // get path of shell executable and store
  char buf[PATH_MAX];
  g_shell_path = realpath(argv[0], buf);

  // ctrl-c handler
  struct sigaction signalAction;
  signalAction.sa_handler = ctr_handler;
  sigemptyset(&signalAction.sa_mask);
  signalAction.sa_flags = SA_RESTART;
  int error = sigaction( SIGINT, &signalAction, NULL );
  if (error) {
    perror ( "sigaction" );
    exit( -1 );
  }

  // zombie process handler
  struct sigaction sigZombie;
  sigZombie.sa_handler = zombie_handler;
  sigemptyset(&sigZombie.sa_mask);
  sigZombie.sa_flags = SA_RESTART;
  error = sigaction(SIGCHLD, &sigZombie, NULL);
  if (error) {
    perror("sigaction");
    exit(-1);
  }

  FILE * fd = fopen(".shellrc", "r");
  if (fd) {
    yyrestart(fd);
    yyparse();
    yyrestart(stdin);
    fclose(fd);
  }

  yyparse();
}

Command Shell::_currentCommand;

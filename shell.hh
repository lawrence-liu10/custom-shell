#ifndef shell_hh
#define shell_hh

#include "command.hh"

struct Shell {

  static void prompt();

  static Command _currentCommand;
};

extern int g_last_status;
extern pid_t g_last_bgpid;
extern std::string g_last_arg;
extern std::string g_shell_path;

#endif

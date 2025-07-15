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

#include "command.hh"
#include "shell.hh"


Command::Command() {
    // Initialize a new vector of Simple Commands
    _simpleCommands = std::vector<SimpleCommand *>();

    _outFile = NULL;
    _inFile = NULL;
    _errFile = NULL;
    _background = false;
    _append = false;
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

    // Print contents of Command data structure
    print();

    // Add execution here
    // For every simple command fork a new process
    // Setup i/o redirection
    // and call exec

    // save default in, out, and err
    int defaultin = dup( 0 );
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

    // create err file for RW if directing, and check if appending
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
        if (pid == 0) { //child, close the fds
            close(defaultin);
            close(defaultout);
            close(defaulterr);

            // read the args and execute
            std::vector<const char *> args;
            for (size_t j = 0; j < _simpleCommands[i]->_arguments.size(); j++) {
                args.push_back(_simpleCommands[i]->_arguments[j]->c_str());
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

    dup2(defaultin, 0);
    dup2(defaultout, 1);
    dup2(defaulterr, 2);
    close(defaultin);
    close(defaultout);
    close(defaulterr);

    if (!_background) {
        for (pid_t pid : childPids) {
            waitpid(pid, nullptr, 0);
        }
    }

    // Clear to prepare for next command
    clear();

    // Print new prompt
    Shell::prompt();
}

SimpleCommand * Command::_currentSimpleCommand;


/*
 *
 * CS-252 
 * shell.l: lexical analyzer for shell
 * You have to extend it.
 *
 */

%{

#include <cstring>
#include "y.tab.hh"

static  void yyunput (int c,char *buf_ptr  );

void myunputc(int c) {
  unput(c);
}

%}

%option noyywrap
%x ENV_MODE


%%
([$])(?=[^\(]) { // matches a $ that doesn’t come before a (
  BEGIN(ENV_MODE); // activate ENV_MODE
}

<ENV_MODE>[^ \n\t] {
  yyval.cpp_string = new std::string(yytext);
  //... code for environment variable substitution ...
  BEGIN(INITIAL); // everything goes back to normal
  return WORD;
}

[^ \n\t] { // won't match while %x is active
  yyval.cpp_string = new std::string(yytext);
  return WORD;
}

\n {
  return NEWLINE;
}

[ \t] {
  /* Discard spaces and tabs */
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


[^ \t\n][^ \t\n]*  {
  /* Assume that file names have only alpha chars */
  yylval.cpp_string = new std::string(yytext);
  return WORD;
}


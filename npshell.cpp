//#define _CRT_SECURE_NO_WARNINGS
# include <stdio.h>
# include <iostream> // cout, endl
# include <string> // string, find_last_of, substr
# include <string.h> // string, find_last_of, substr
# include <vector> // vector, push_back
# include <stdlib.h> // setenv() getenv()
# include <cstdlib> // system, atoi
# include <ctype.h> // isdigit() 
# include <cstdio> // system, atoi
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>

using namespace std;

struct command_node{
    char ** cstr ;
    char type ; 
    int num ;
};

struct fd_node{
  int readnum ;
  int writenum ;
  int count ;
};

// s for simple task 
// p for simple pipe 
// n for number pipe
// f for file_rediration
// e for (!)number pipe

void npshell();
void Setenv(char * var, char * value) ;
void printenv( char * var) ;
void init_env() ;
void parser(char* linestr);
bool is_pipe(char* str, int &pipenum, bool &is_mark) ;
bool is_rediration( char* str) ;
bool tonewpipe( int current) ;
int is_build_in( char * str ) ;
void depatcher(char** input);
void simpletask(int current);
void sigchld_handler( int sig ) ;
void execute() ;
void numberpipe(int read_end, int write_end, int current) ;
void checkfdlist() ;
void checkfdlist_numberpipe() ;
bool tonew_numberpipe( int count, int &read, int &write ) ;
void close_numberpipe() ;
void adjust_fd_list() ;
void maintain_fd_list() ; // for parent

vector<command_node>command_list;
vector<fd_node>fd_list ;

int main() {
    init_env() ;
    npshell();
} // main()

void init_env() {
  setenv( "PATH", "bin:.", 1 ) ;
} // init_env()


void npshell() {
    int status = 0;
    int i = 0 ;
    int size = 0 ;
    char* line = (char*)malloc(sizeof(char) * 15001);
    command_list.clear() ;
    fd_list.clear() ;
    while (1) {
        cout << "% ";
        if ( fgets( line, 15001, stdin ) != NULL ) {
          if ( strcmp( line, "\n") != 0 ){
            adjust_fd_list() ;
            parser(line);
          } // if   
        } // if   
        else 
          break;
        free(line) ;
        line = (char*)malloc(sizeof(char) * 15001);
    } // while
    
    free(line) ;
} //npshell()

void parser(char* linestr) {
    const char* del = "\f\r\v\n\t ";
    char** argv = (char**)malloc(sizeof(char*) * 4000 ); // 4000隨便給的
    int i = 0;
    int j = 0;
    int pipe_num = 0;
    char * token = strtok(linestr, del);
    while (token != NULL) {
        argv[i] = token;
        token = strtok(NULL, del);
        i = i + 1;
    } // while

    argv[i] = token; // 給予最後一個NULL
    if ( is_build_in(argv[0]) == 1 )
      Setenv( argv[1], argv[2] ) ;
    else if ( is_build_in(argv[0]) == 2 )
      printenv( argv[1] ) ;
    else if ( is_build_in(argv[0]) == 3 )
      exit(0) ;
    else if ( is_build_in(argv[0]) == 0 ) 
      depatcher(argv);   
    else
      ;
    free(argv) ;

} // parser

void depatcher(char** input) {
    int i = 0 ;
    int j = 0;
    int k = 0 ;
    int pipe_num = 0 ;
    int command_num = 0;
    int singlepipe_num = 0;
    bool mark = false ;
    command_node temp ;
    char** commands = (char**)malloc(sizeof(char*) * 8);
    command_list.clear();
    while (input[i] != NULL) {
        while ( input[i] != NULL && is_pipe(input[i], pipe_num, mark) == false
                && is_rediration(input[i]) == false ) {
            commands[j] = input[i];
            i = i + 1;
            j = j + 1;
        } // while
        
        if (input[i] == NULL) {
            commands[j] = input[i];
            temp.cstr = commands ;
            temp.type = 's' ;
            temp.num = 0 ;
            command_list.push_back(temp);
            command_num = command_num + 1;
        } // if 
        else if (is_pipe(input[i], pipe_num, mark) == true && pipe_num == 0) {
            singlepipe_num = singlepipe_num + 1 ;
            commands[j] = NULL;
            temp.cstr = commands ;
            temp.type = 'p' ;
            temp.num = 0 ;
            command_list.push_back(temp);
            command_num = command_num + 1;
            i = i + 1;
        } // else if 
        else if (is_pipe(input[i], pipe_num, mark) == true && mark == false && pipe_num > 0) {
            commands[j] = NULL;
            temp.cstr = commands ;
            temp.type = 'n' ;
            temp.num = pipe_num ;
            command_list.push_back(temp);
            command_num = command_num + 1;
            i = i + 1;
        } // else if
        else if (is_pipe(input[i], pipe_num, mark) == true && mark == true && pipe_num > 0) {
            commands[j] = NULL;
            temp.cstr = commands ;
            temp.type = 'e' ;
            temp.num = pipe_num ;
            command_list.push_back(temp);
            command_num = command_num + 1;
            i = i + 1;
        } // else if
        else if ( is_rediration(input[i]) == true ) {
          i = i + 1 ; // let ">" get the fuckoff
          if ( input[i] != NULL ) {
            commands[j] = input[i] ;
            commands[j+1] = NULL ;
            temp.cstr = commands ;
            temp.type = 'f' ;
            temp.num = 0 ;
            command_list.push_back(temp);
            command_num = command_num + 1 ;
            i = i + 1 ;
          } // if 
          else 
            fprintf( stderr, "input[i] is NULL!" ) ; 
        } // else if 
        
        j = 0;
        commands = (char**)malloc(sizeof(char*) * 8);
    } // while

    execute() ;
    while ( k < command_list.size() ) {
      char ** to_del = command_list[k].cstr ;
      free(to_del) ; 
      k = k + 1 ;
    } // while

    free(commands) ;
    command_list.clear();

} // depatcher()

void execute(){
  int pipenum = 0 ;
  int i = 0 ;
  int j = 0 ;
  pid_t pid  ;
  bool newpipe = true ;
  fd_node temp ;
  int totalnum = command_list.size() ;
  while ( i < totalnum ) {
    if ( command_list[i].type == 'p' && i != totalnum - 1 )
      pipenum = pipenum + 1 ;
    i = i + 1 ;
  } // while 
  int redir_fd = -1 ;
  int pipe_count = 0 ;
  int current_num = 0 ;
  int pipes[ pipenum * 2 ] ;
  int limit_num = 0 ;
  int numpipes[2] ; // for numberpipe

  signal( SIGCHLD, sigchld_handler) ; // its just like turn on interrupt

  while ( current_num < totalnum ) {
    newpipe = tonewpipe(current_num) ;
    if ( pid != -1 && current_num != 0 && (command_list[current_num-1].type == 'n' 
                              || command_list[current_num-1].type == 'e') )
      adjust_fd_list() ;

    if ( pid != -1 && current_num != totalnum - 1 && newpipe == true ) { // to new a pipe for normal pipe
      if ( pipe( pipes + pipe_count*2 ) < 0 )
        fprintf( stderr, "pipe error! number:%d\n", pipe_count ) ; 
    } // if
    
    if ( pid != -1 && (command_list[current_num].type == 'n' || command_list[current_num].type == 'e')){ // to new a pipe for numberpipe 
      if ( tonew_numberpipe(command_list[current_num].num, numpipes[0], numpipes[1]) == true ) {
        if ( pipe(numpipes) < 0 ) 
          fprintf( stderr, "numpipe error! command_number:%d\n", current_num ) ;
        temp.readnum = numpipes[0] ;
        temp.writenum = numpipes[1] ;
        temp.count = command_list[current_num].num ;
        fd_list.push_back(temp) ;
      } // if
    } // if
    
    pid = fork() ;
    if ( pid == -1 ) {
      wait(NULL) ;
    } // if 
    else if ( pid == 0 ) { // childprocess
      if ( command_list[current_num].type == 's' ) { // SINGLE WORK
        if (current_num == 0) {
          checkfdlist() ;
        } // if 
        else if (command_list[current_num-1].type == 'p' ) {
          close_numberpipe() ;
          dup2( pipes[(pipenum-1)*2], STDIN_FILENO ) ;
          limit_num = pipe_count * 2 ; 
          i = 0 ;
          while ( i < limit_num ) {
            close(pipes[i]) ; 
            i = i + 1 ;
          } // while 
        } // else if 
        else if (command_list[current_num-1].type == 'n'|| command_list[current_num-1].type == 'e' ) {
          checkfdlist() ;
        } // else if

        simpletask(current_num) ; 
      } // if
      else if ( command_list[current_num].type == 'n' || command_list[current_num].type == 'e' ) { // NUMBER PIPE
        if ( current_num - 1 >= 0 && command_list[current_num-1].type == 'p' ) { // before command is normal pipe
          int np_pipein = ( pipe_count-1 ) * 2 ;
          dup2( pipes[np_pipein], STDIN_FILENO) ;
          close(pipes[np_pipein]) ;
          close(pipes[np_pipein+1]) ;
        } // if
        else {// before command is numberpipe or its is first command
          checkfdlist_numberpipe() ;
        } // else

        numberpipe(numpipes[0], numpipes[1], current_num) ;
      } // else if 
      else if ( command_list[current_num].type == 'p') { // NORMAL PIPE
        if ( pipe_count == 0 ) { // first time normal pipe
          checkfdlist() ;
          dup2( pipes[1], STDOUT_FILENO ) ;
        } // if    
        else if ( pipe_count != 0 && command_list[current_num-1].type == 'p') { // before command is normal pipe
          close_numberpipe() ;
          int pipein = ( pipe_count-1 ) * 2 ;
          int pipeout = pipein + 3 ;
          dup2( pipes[pipein], STDIN_FILENO ) ;
          dup2( pipes[pipeout], STDOUT_FILENO ) ;
        } // else if
        else if ( pipe_count != 0 && (command_list[current_num-1].type == 'n' || command_list[current_num-1].type == 'e')) { // before command is number pipe 
          checkfdlist() ;
          dup2( pipes[(pipe_count*2)+1], STDOUT_FILENO ) ;
        } // else if

        i = 0 ;
        limit_num = (pipe_count+1) * 2 ; // to close new create pipe  
        while ( i < limit_num ) {
          close(pipes[i]) ; 
          i = i + 1 ;
        } // while 
        simpletask(current_num) ;  
      } // else if
      else if ( command_list[current_num].type == 'f' ) { // FILE REDIRATION
        j = 0 ;
        while ( command_list[current_num].cstr[j] != NULL )
          j = j + 1 ;
        // this line just for good looking
        redir_fd = open( command_list[current_num].cstr[j-1], O_TRUNC | O_CREAT | O_WRONLY, 0644 ) ;
        command_list[current_num].cstr[j-1] = NULL ;
        if ( current_num == 0 ) {
          checkfdlist() ;
          dup2( redir_fd, STDOUT_FILENO ) ;
        } // if   
        else if (command_list[current_num-1].type == 'p') { // means it is followed pipe
          int redir_pipein = ( pipe_count-1 ) * 2 ;
          dup2( pipes[redir_pipein], STDIN_FILENO ) ;
          dup2( redir_fd, STDOUT_FILENO ) ;
          limit_num = pipe_count * 2 ; 
          i = 0 ;
          while ( i < limit_num ) {
            close(pipes[i]) ; 
            i = i + 1 ;
          } // while 
        } // else if 
        else if ( command_list[current_num-1].type == 'n'|| command_list[current_num-1].type == 'e') { // means it is followed number pipe
          checkfdlist() ;
          dup2( redir_fd, STDOUT_FILENO ) ;
        } // else if 

        close(redir_fd) ;
        simpletask(current_num) ;
      } // else if 
    } // else if
    else { // parentprocess
      maintain_fd_list() ;
      if ( current_num != 0 && command_list[current_num-1].type == 'p' ) {
        close(pipes[(pipe_count-1)*2]) ;
        close(pipes[(pipe_count-1)*2 + 1 ]) ;
      } // if
    } // else 
    
    if ( pid != -1 && newpipe == true )
      pipe_count = pipe_count + 1 ;
    if ( pid != -1 )
      current_num = current_num + 1 ;
  } // while

  if ( command_list[current_num-1].type == 'n' || command_list[current_num-1].type == 'e'  )
    ;
  else 
    waitpid(pid, NULL, 0) ;
} // execute()

void numberpipe(int read_end, int write_end, int current) {
  dup2( write_end, STDOUT_FILENO) ;
  if (command_list[current].type == 'e') {
    dup2( write_end, STDERR_FILENO) ;
  } // if
  close(read_end) ;
  close(write_end) ;
  close_numberpipe() ;
  simpletask(current) ;

} //numberpipe()

void checkfdlist() {
  int size = fd_list.size() ;
  int i = 0 ;
  while ( i < size ) {
    if ( fd_list[i].count == 0 ) {
      dup2(fd_list[i].readnum, STDIN_FILENO) ;
      close(fd_list[i].readnum) ;
      close(fd_list[i].writenum) ;
      fd_list.erase(fd_list.begin()+i) ;
      break ;
    } //if 
    i = i + 1 ;
  } // while 

  close_numberpipe() ;
} // checkfdlist()

void checkfdlist_numberpipe() {
  int size = fd_list.size() ;
  int i = 0 ;
  while ( i < size ) {
    if ( fd_list[i].count == 0 ) {
      dup2(fd_list[i].readnum, STDIN_FILENO) ;
      close(fd_list[i].readnum) ;
      close(fd_list[i].writenum) ;
      fd_list.erase(fd_list.begin()+i) ;
      break ;
    } //if 
    i = i + 1 ;
  } // while 
} //checkfdlist_numberpipe()

void close_numberpipe() {
  int i = 0 ;
  while( i < fd_list.size() ) {
    close(fd_list[i].readnum) ;
    close(fd_list[i].writenum) ; 
    i = i + 1;
  } // while

} // close_numberpipe()

void adjust_fd_list() {
  int i = 0 ;
  while ( i < fd_list.size() ) {
    fd_list[i].count = fd_list[i].count - 1 ;
    i = i + 1 ;
  } // while

} // adjust_fd_list()

void maintain_fd_list() {
  int i = 0 ;
  while ( i < fd_list.size() ) {
    if ( fd_list[i].count == 0 ) {
      close(fd_list[i].readnum) ;
      close(fd_list[i].writenum) ;
      fd_list.erase(fd_list.begin()+i) ;
      break ;
    } // if 
    i = i + 1 ;
  } // while 

} //maintain_fd_list()

bool is_pipe(char* str, int &pipenum, bool &is_mark) {
    char temp[256];
    const char* del = "!|";
    bool ans = false;
    is_mark = false ;
    pipenum = 0;
    strcpy(temp, str);
    if (temp[0] == '|' || temp[0] == '!') {
      if (temp[0] == '!')
        is_mark = true ;
      ans = true;
      char* token = strtok(str, del);
      if ( token != NULL )
        pipenum = atoi(token);
    } // if 

    return ans;
    
} // is_pipe()

bool tonew_numberpipe( int count, int &read, int &write ){
  bool ans = true ;
  int i = 0 ;
  read = -1 ;
  write = -1 ;
  while ( i < fd_list.size() ) {
    if (fd_list[i].count == count ) {
      ans = false ;
      read = fd_list[i].readnum ;
      write = fd_list[i].writenum ; 
      break ;
    } // if   
    i = i + 1 ;
  } // while 

  return ans ;
} // tonew_numberpipe()

bool is_rediration( char* str){
  if ( str != NULL ) {
    if ( strcmp( str, ">") == 0 )
      return true ;
  } // if 
  else 
    fprintf( stderr, "is_refiration error str is NULL!" ) ;

  return false ;
} // is_rediration()

bool tonewpipe( int current) {
  if ( command_list[current].type == 'f' || command_list[current].type == 'n' 
       || command_list[current].type == 's' || command_list[current].type == 'e' )
    return false ;

  return true ;
} // tonewpipe()

int is_build_in( char * str ) {
  if ( str ==  NULL ) {
    fprintf( stderr, "is_build_in error str is NULL!" ) ;  
    return -1 ;
  } // if   
  else if ( strcmp( str, "setenv") == 0 )
    return 1 ; // 1 means setenv
  else if ( strcmp( str, "printenv") == 0 )
    return 2 ; // 2 means printenv
  else if ( strcmp( str, "exit") == 0 )
    return 3 ; // 3 means exit
  else
    return 0 ; 

} // is_buid_in()

void simpletask(int current) {
  if ( execvp( command_list[current].cstr[0], command_list[current].cstr) == -1 ) {
    fprintf( stderr, "Unknown command: [%s].\n", command_list[current].cstr[0] ) ;
    exit(0) ;
  } // if 
} //simpletask()

void sigchld_handler( int sig ) { //for zombie process
  wait(NULL) ;
} //sigchld_handler()

void Setenv(char * var, char * value ) {
  setenv( var, value, 1 ) ;
} // setenv()

void printenv( char * var ) {
  char * str = getenv( var ) ;
  if ( str != NULL )
    printf( "%s\n", str ) ; 
} // printenv()



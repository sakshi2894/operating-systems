# Assignment 2a
Submitted By: Sakshi Bansal, campus id: 9081457781

The submission contains one executable named smash.c which is _command line interpreter (CLI)_ or, as it is more commonly known, a _shell_. The shell operates in a basic way: when you type in a command (in response to its prompt), the shell creates a child process that executes the command you entered and then prompts for more user input when it has finished.

### Specifications
 `smash`  (short for Super Madison Shell, naturally), is basically an interactive loop: it repeatedly prints a prompt  `smash>` , parses the input, executes the command specified on that line of input, and waits for the command to finish. This is repeated until the user types  `exit`.

The shell can be invoked with either no arguments or a single argument; anything else is an error. Here is the no-argument way:

```
prompt> ./smash
smash> 
```

At this point,  `smash`  is running, and ready to accept commands. Type away!

The mode above is called  _interactive_  mode, and allows the user to type commands directly. The shell also supports a  _batch mode_, which instead reads input from a batch file and executes commands from therein.  In batch mode, no prompt is printed. Here is how you run the shell with a batch file named  `batch.txt`:

```
prompt> ./smash batch.txt
```

### Built-in Commands

Whenever the shell accepts a command, it checks whether the command is a  **built-in command**  or not. If it is, it's not executed like other programs. Instead, the shell invokes its own implementation of the built-in command. For example, to implement the  `exit`  built-in command, the program simply calls  `exit(0);`  in the source code, which then will exit the shell.

In this project, the commands `exit`,  `cd`, and  `path`  are implemented as built-in commands.

-   `exit`: When the user types  `exit`, the shell simply calls the  `exit`  system call with 0 as a parameter. It is an error to pass any arguments to  `exit`.
    
-   `cd`:  `cd`  always takes one argument (0 or >1 args should be signaled as an error). To change directories, the  `chdir()`  system call is used with the argument supplied by the user; if  `chdir`  fails, that is also an error.
    
-   `path`: The  `path`  command takes 1 or more arguments, with each argument separated by whitespace from the others. Three options are supported:  `add`,  `remove`, and  `clear`.  Invalid arguments causes an error and prevents the entire line from executing. 
    
    -   `add`  accepts 1 path. The shell appends it to the  _beginning_  of the path list. For example,  `path add /usr/bin`  results in the path list containing  `/usr/bin`  and  `/bin` . No error is  reported if an invalid path is added.
    -   `remove`  accepts 1 path. It searches through the current path list and removes the corresponding one. If the path cannot be found, this is an error.
    -   `clear`  takes no additional argument. It simply removes everything from the path list. If the user sets path to be empty, then the shell will not be able to run any programs (except built-in commands).


### Redirection
Many times, a shell user prefers to send the output of a program to a file rather than to the screen. Usually, a shell provides this nice feature with the  `>`  character. Formally this is named as redirection of standard output.

For example, if a user types  `ls -la /tmp > output`, nothing should be printed on the screen. Instead, the standard output of the  `ls`  program should be rerouted to the file  `output`. In addition, the standard error output of the program should be rerouted to the file  `output`. However, if the program cannot be found (i.e., mistyped  `pwd`  as  `pdd`), an error should be reported, but not to be redirected to  `output`.

If the  `output`  file exists before you run your program, it is simply overwritten. 

The exact format of redirection is a command (and possibly some arguments) followed by the redirection symbol followed by a filename. Multiple redirection operators or multiple files to the right of the redirection sign are errors. Redirection without a command is also not allowed - an error should be printed out, instead of being redirected.

Note: The program doesn't handle redirection for built-in commands.

### Parallel Commands

Smash allows the user to launch parallel commands. This is accomplished with the ampersand operator as follows:

```
smash> cmd1 & cmd2 args1 args2 & cmd3 args1
```

In this case, instead of running  `cmd1`  and then waiting for it to finish, the shell would run  `cmd1`,  `cmd2`, and  `cmd3`  (each with whatever arguments the user has passed to it) in parallel,  _before_  waiting for any of them to complete. 

Then, after starting all such processes, parent process waits for all child processes to complete using `WAIT` call. After all processes are done, the control is returned to the user as usual (or, if in batch mode, moves on to the next line).

Note:
-   The program doesn't handle parallel built-in commands
-   Redirection is supported (e.g.,  `cmd1 > output & cmd 2`).
-   Empty commands are allowed (i.e.,  `cmd &`,  `& cmd`).

### Multiple Commands

What if a shell user would like to type in multiple commands in a single line? Sometimes, they might turn in a long list of commands, prepare some popcorns and, wait until all of them to finish. This is supported by semicolons:

```
smash> cmd1 & cmd2 args1 args2 ; cmd3 args1
```

Here, our shell runs  `cmd 1`  and  `cmd 2`  in parallel, like specified above, waits until they complete, and executes  `cmd 3`  afterwards.

Note:
-   The shell supports multiple built-in commands, such as  `ls ; cd foo ; ls`
-   Redirection is supported (e.g.,  `cmd1 > output ; cmd 2`).
-   Empty commands are allowed (i.e.,  `cmd ;`,  `; cmd &`).


### Error Handling
The error message ```An error has occurred``` should be printed to stderr (standard error), in case of ANY error/exception. 

After most errors, the shell simply  _continue processing_  after printing the one and only error message. However, if the shell is invoked with more than one file, or if the shell is passed a file that doesn’t exist, it exits by calling  `exit(1)`.

There is a difference between errors that the shell catches and those that the program catches. The shell catches all the syntax errors specified in this project page. If the syntax of the command looks perfect, the shell simply runs the specified program. Syntax checking in this project includes invalid arguments to built-in commands and invalid use of redirection operators. If there are any program-related errors (e.g., invalid arguments to  `ls`  when you run it, for example), the shell does not have to worry about that (rather, the program will print its own error messages and exit).

For parallel and multiple commands, syntax errors (e.g.,  `ls; > output`) or invalid programs names (e.g., a mistyped  `ls`, like  `lss`) should prevent the entire line from executing.

### Implementation

The shell runs in a while loop, repeatedly asking for input to tell it what command to execute. The shell provides its own implementation of Built-in commands and looks for the required binary for all the other commands. An array of paths internally stores all the paths where shell should look for a binary. For example, if the user types  `ls -la /tmp`, the shell should run the program  `/bin/ls`  with the given arguments  `-la`  and  `/tmp`  (path = ["`/bin`"]). The loop continues indefinitely, until the user types the built-in command  `exit`, at which point it exits. 

For reading lines of input in  _interactive mode_ , I have used  `getline()`. This allows us to obtain arbitrarily long input lines with ease. Generally, the shell will be run in  where the user types a command (one at a time) and the shell acts on it. However, our shell also supports  _batch mode_, in which the shell is given an input file of commands; in this case, the shell does not read user input (from  `stdin`) but rather from this file to get the commands to execute.

The shell first parses the commands, and upon encountering any syntax error, prints out the error message. To parse the input line into constituent pieces, I have used`strsep()`.   To execute commands, the parent process uses `fork()` to spin a child process and executes it using  `execv()`, and waits for ALL child processes using  `wait()` before moving on to the next command.

In either mode, if the end-of-file marker (EOF) is hit, `exit(0)`  is called and the process exits gracefully.


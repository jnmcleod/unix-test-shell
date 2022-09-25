# unix-test-shell

Unix Shell program

Operates like typical shells (ksh, zsh), allowing the user to enter commands to the operating system

Follows a tokenize -> interpret -> execute structure.  Commands are read in and broken up into components.  
Then, each is parsed to determine if a valid command was provided and if so, what the arguments are
Then the appropriate function is called to handle the command
If any errors occur during this process, an exception is passed up the chain to the main driver, which handles resets back to a safe state

Current internal commands:

1. newname alias [argument]
Adds or deletes an alias.  The first argument is the name of the alias and the second is an optional value.
If one argument is included, that alias will be deleted from the alias list.  If two argumentss are included, the first is inserted into the list as an alias for the second

2. "-" command
runs a command as a background process

3. backjobs
Prints status info about current background jobs.  Accepts no arguments

4. frontjob jobID
Brings a background job to the foreground.
jobID must be an integer.  Use the backjobs command to get the jobIDs of current background jobs

5. [not]cond ( condition filename ) command
Conditionally executes a command.  If cond is used, the condition must evaluate to true for the command to execute.  If notcond is used, the condition must evaluate to false for the command to execute.
Accepts the following formats:
[not]cond ( condition filename ) command
[not]cond (condition filename) command
[not]cond condition filename command
Acceptable conditions are checke, checkd, checkr, checkw and checkx

6. savenewnames filename
Saves the current alias list to the given file.  If file does not exist, it will be created

7. readnewnames filename
Reads the specified file into the alias list, updating any with new values and adding any new aliases.  Does not delete any other aliases in the current list.
File must exist and be readable

8. history
Prints the last 10 commands entered at the command line.  Accepts no arguments

9. setshellname name
Sets the shell name.  Also saves this value to config.ini so it is maintained between sessions.  Only accepts one argument.

10. setterminator delim
Sets the shell delimiter.  Also saves this value to config.ini, so it maintained between sessions.  Only accepts one argument.

11. newnames
Prints the current alias list.  Accepts no arguments

12. ! argument
Reruns the line of history specified by argument.  Argument must be numeric

13. usescript filename
reads a list of commands from the given file

14. stop
exits the shell

Also allows reading from or writing to a file with the [ and ] tokens, respectively

//
//  main.cpp


//***********NOTE********
//The usescript command prints out the command being executed
//so it isn't a "silent" run - even commands with no output still take up space in this script
//to help organize the output, I added "output *************" commands after each script finishes executing



#include "shell.hpp"

//constant global variables for default startup values
const std::string defaultName = "toyshell";
const std::string defaultDelim = ">";
const int defaultAliasSize = 10;
const int defaultHistorySize = 10;

//between file redirection and piping, there's a decent chance we will lose std in or out between runs
//these global constants are used to save both
const int STDIN_COPY = dup(STDIN_FILENO);
const int STDOUT_COPY = dup(STDOUT_FILENO);

int main(int argc, const char * argv[])
{
    Shell currentShell(defaultName, defaultDelim, defaultAliasSize, defaultHistorySize);
    
    //infinite loop, since exit conditions are handled internally
    while (true)
    {
        try
        {
            currentShell.run();
        }
        catch (error const &e)
        {
            //file descriptors might be off, so we immediately reset stdin and stdout
            dup2(STDIN_COPY, STDIN_FILENO);
            dup2(STDOUT_COPY, STDOUT_FILENO);
            
            switch (e.errorCode)
            {
                case RETURNCODE::EXIT:
                    return 0; //normal exit
                    
                case RETURNCODE::TOO_FEW_ARGS:
                    std::cout << "Too few arguments\n";
                    break;
                    
                case RETURNCODE::TOO_MANY_ARGS:
                    std::cout << "Too many arguments\n";
                    break;
                    
                case RETURNCODE::INVALID_ARG:
                    std::cout << "Invalid argument\n";
                    break;
                    
                case RETURNCODE::NO_HISTORY:
                    std::cout << "Requested line of history does not exist\n";
                    break;
                    
                case RETURNCODE::NO_ALIAS:
                    std::cout << "Alias not found\n";
                    break;
                    
                case RETURNCODE::NO_OVERRIDE:
                    std::cout << "Cannot override default commands\n";
                    break;
                    
                case RETURNCODE::RECURSIVE_ALIAS:
                    std::cout << "Alias is recursive; unable to resolve\n";
                    break;
                    
                case RETURNCODE::FILE_ERROR:
                    std::cout << "Unable to open file\n";
                    break;
                    
                case RETURNCODE::NO_DELETE:
                    std::cout << "rm command disabled for safety\n";
                    break;
                    
                case RETURNCODE::BAD_FORMAT:
                    std::cout << "Alias file is in incorrect format\n";
                    break;
                    
                case RETURNCODE::COMMAND_DNE:
                    std::cout << "Command does not exist\n";
                    break;
                    
                case RETURNCODE::NO_JOB:
                    std::cout << "No job exists with that ID\n";
                    break;
                    
                case RETURNCODE::BAD_SYNTAX:
                    std::cout << "Invalid syntax\n";
                    break;
                    
                case RETURNCODE::PROCESS_ERROR:
                    perror("Error in child process");
                    break;
                    
                case RETURNCODE::CMD_NOT_FOUND:
                    std::cout << "Linux command not found\n";
                    break;
                    
                case RETURNCODE::RECURSIVE_SCRIPT:
                    std::cout << "Recursion in script.  Exiting script mode\n";
                    break;
                    
                case RETURNCODE::OUTPUT_COMMAND:
                    //not really an error, like EXIT it just signals that the current cycle is finished
                    break;
                    
                case RETURNCODE::RECURSIVE_REDIRECTION:
                    std::cout << "Cannot read and write to the same file\n";
                    break;
                    
                default:
                    std::cout << "Uncaught exception " << (int) e.errorCode << "\nQuitting\n";
                    return (int) e.errorCode;
            }
        }
        //regardless of exit status of last command, the shell must be cleared before the next loop
        currentShell.reset();
        dup2(STDIN_COPY, STDIN_FILENO);
        dup2(STDOUT_COPY, STDOUT_FILENO);
    }
    return 0;
}

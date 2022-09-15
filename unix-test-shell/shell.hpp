//  shell.hpp


#ifndef shell_hpp
#define shell_hpp

#include <string.h> //for strdup
#include <iostream>
#include <fstream>  //for c++ file input/output
#include <fcntl.h>  //for c file input/output, needed for dup2
#include <sstream>  //for istringstream, used to parse tokens
#include <deque> //my primary data structure
#include <map>  //used for function address maps
#include <forward_list> //used in Shell::parseAliases()
#include <regex>  //used to parse whitespace, only works for gcc version > 4.9
#include <unistd.h>
#include <ctime>
#include <iomanip>
#include <sys/wait.h>  //older versions of gcc don't seem to know how to handle the return value if this isn't included, even though the return value is just an int
#include <sys/stat.h>
#include <limits>

extern char** environ;

//Internal error codes
enum class RETURNCODE {EXIT, TOO_FEW_ARGS, TOO_MANY_ARGS, INVALID_ARG, NO_HISTORY, NO_ALIAS, NO_OVERRIDE, RECURSIVE_ALIAS, NO_DELETE, FILE_ERROR, BAD_FORMAT, COMMAND_DNE, BAD_SYNTAX, NO_JOB, PROCESS_ERROR, CMD_NOT_FOUND, RECURSIVE_SCRIPT, OUTPUT_COMMAND, RECURSIVE_REDIRECTION};

//wrapper for my error codes, so that I can throw them as an exception rather than trying to handle return values
struct error : public std::exception
{
    RETURNCODE errorCode;
    error(RETURNCODE e) : errorCode(e) {}
};

/*struct to hold details for each background job */
struct bgJob
{
    int jobID;
    std::vector<pid_t> pidList;
    std::string cmd;
    time_t startTime;
    
    bgJob(int i, std::vector<pid_t> p, std::string c, time_t t): jobID(i), pidList(p), cmd(c), startTime(t){}
};

/*info for the man command when applied to internal commands
* since these are simple commands, I'm just putting the strings here rather than having an actual file */
const std::string NEWNAMEINFO = "newname usage:\nnewname alias [value]\nAdds or deletes an alias.  The first argument is the name of the alias and the second is an optional value.\nIf one arg is included, that alias will be deleted from the alias list.  If two args are included, the first is inserted into the list as an alias for the second\n";
const std::string FRONTJOBINFO = "frontjob usage:\nfrontjob jobid\nBrings a background job to the foreground.\njobid must be an integer.  Use the backjobs command to get the jobid's of current background jobs\n";
const std::string BACKJOBINFO = "backjobs usage:\nbackjobs\nPrints status info about current background jobs.  Accepts no arguments\n";
const std::string CONDINFO = "cond and notcond usage:\n[not]cond ( condition file ) command\nConditionally executes a command.  If cond is used, the condition must evaluate to true for the command to execute.  If notcond is used, the condition must evaluate to false for the command to execute.\nAccepts the following formats:\n[not]cond ( condition file ) command\n[not]cond (condition file) command\n[not]cond condition file command\nAcceptable conditions are checke, checkd, checkr, checkw and checkx\n";
const std::string SAVEALIASINFO = "savenewnames usage:\nsavenewnames file\nSaves the current alias list to the given file.  If file does not exist, it will be created\n";
const std::string READALIASINFO = "readnewnames usage:\nreadnewnames file\nReads the specified file into the alias list, updating any with new values and adding any new aliases.  Does not delete any other aliases in the current list.\nFile must exist and be readable\n";
const std::string HISTORYINFO = "history usage:\nhistory\nPrints the last 10 commands entered at the command line.  Accepts no arguments\n";
const std::string SETSHELLINFO = "setshellname usage:\nsetshellname name\nSets the shell name.  Also saves this value to config.ini so it is maintained between sessions.  Only accepts one argument.\n";
const std::string SETDELIMINFO = "setterminator usage:\nsetterminator delim\nSets the shell delimiter.  Also saves this value to config.ini, so it maintained between sessions.  Only accepts one argument.\n";
const std::string PRINTALIASINFO = "newnames usage:\nnewnames\nPrints the current alias list.  Accepts no arguments\n";
const std::string REPLACEHISTINFO = "! usage:\n! arg\nReruns the line of history specified by arg.  Arg must be numeric\n";


class Shell
{
private:
    //defines a "string to function address" pair
    typedef std::pair<std::string, void(*)(Shell*)> functionPair; //used in the map later, to improve readability
    
    std::map<std::string, void(*)(Shell*)> commandMap; //maps from the command to the address of the function to run
    void (*functionPointer)(Shell*); //declaring a function pointer with a Shell pointer parameter
    
    std::string shellName;
    std::string shellDelimiter;
    int commandCount;
    
    int maxAliasSize, maxHistorySize;
    std::deque<std::deque<std:: string>> aliasList;
    std::deque<std::string> historyList;
    bool NOHISTORYFLAG;
    std::string currentLine;  //the current command, including pipes and redirection, but with no leading or trailing spaces
    std::deque<std::string> tokenList;
    
    std::deque<std::deque<std::string>> childSubCommands; //used for linux piping
    
    bool backgroundMode;
    int bgJobCount;
    std::map<int, bgJob> bgJobQueue; //holds all jobs currently running in the background
    
    std::deque<std::deque<std::string>> scriptStack; //stack of queues that holds any currently executing scripts
    
    std::map<std::string, std::string> infoMap;
    
    //COMMAND FUNCTIONS
    //declaring all as static because it makes my function pointer mapping method significantly easier
    //as they can be treated and passed around as normal functions rather than member functions
    //the static versions just call the regular ones which do the actual work
    static void staticSetShellName(Shell*);
    static void staticSetShellDelimiter(Shell*);
    static void staticPrintHistory(Shell*);
    static void staticPrintAliases(Shell*);
    static void staticNewnameCommand(Shell*);
    static void staticDeleteAlias(Shell*);
    static void staticAddNewAlias(Shell*);
    static void staticSaveNewAliasFile(Shell*);
    static void staticReadAliasFile(Shell*);
    static void staticRunLinuxCommand(Shell*);
    static void staticInfoCommand(Shell*);
    static void staticExit(Shell*);
    static void staticPrintBGJobs(Shell*);
    static void staticBringJobToFG(Shell*);
    static void staticCondExec(Shell*);
    static void staticReverseCondExec(Shell*);
    static void staticCull(Shell*);
    static void staticUsescript(Shell*);
    static void staticOutput(Shell*);
    
    void setShellName();
    void setShellDelimiter();
    void printHistory();
    void printAliases();
    void newnameCommand();
    void deleteAlias();
    void addNewAlias();
    void saveNewAliasFile();
    void readAliasFile();
    void runChildProcess(int);
    void runLinuxCommand();
    void infoCommand();
    void exit();
    void printBGJobs();
    void bringJobToFG();
    void condExec();
    void reverseCondExec();
    void cull();
    void usescript();
    void output();
    
    //HELPER FUNCTIONS
    void replaceWithHistory();  //the ! # command is special; because it requires substitution of a command from history before following the regular tokenize -> interpret -> execute structure, it is implemented seperate from the other command functions, and runs immediately after reading the input line
    void tokenizeString(std::string, std::deque<std::string>*);
    void parseRedirection();
    void parseCommandLineWhitespace(); //used to remove leading whitespace from command
    void addJobToBGQueue(std::vector<pid_t>);
    int condChecker(); //evaluates conditions, passes back to either cond or reverseCondExec
    
public:
    //INITLIAZATION FUNCTIONS
    //set up variables, default values
    Shell(std::string, std::string, int, int);
    
    //MAIN PROGRAM FUNCTIONS
    void run();  //main driver
    void printCommandLine();  //prints toyshell[1]>
    bool readCommandLine();  //reads input
    void parseCommandLine();  //checks for input errors, tokenizes the input string
    void parseAliases();  //loops through the input string, replaces all aliases, checks for recursive aliasing
    void addCommandToHistory();
    void execCommand();  //selects the right command to run
    void reset(); //clears the token queue before the next command
};


#endif /* shell_hpp */

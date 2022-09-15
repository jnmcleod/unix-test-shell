//  shell.cpp


#include "shell.hpp"

//INITLIAZATION FUNCTIONS
//set up variables, default values
//Does not throw exceptions
Shell::Shell(std::string defaultName, std::string defaultDelim, int aliasSize, int historySize)
{
    //reads startup values from config.ini if it exists; if not this file will be created later
    std::ifstream configFile("config.ini");
    if (!configFile)
    {
        shellName = defaultName;
        shellDelimiter = defaultDelim;
    }
    else
    {
        configFile >> shellName >> shellDelimiter;
    }
    configFile.close();
    
    commandCount = 1;
    maxAliasSize = aliasSize;
    maxHistorySize = historySize;
    NOHISTORYFLAG = false;
    
    //setting up the map from the name of commands to the address of the static function they should run
    commandMap =
    {
        {functionPair("setshellname", &staticSetShellName)},
        {functionPair("setterminator", &staticSetShellDelimiter)},
        {functionPair("history", &staticPrintHistory)},
        {functionPair("newname", &staticNewnameCommand)},
        {functionPair("newnames", &staticPrintAliases)},
        {functionPair("savenewnames", &staticSaveNewAliasFile)},
        {functionPair("readnewnames", &staticReadAliasFile)},
        {functionPair("man", &staticInfoCommand)},
        {functionPair("stop", &staticExit)},
        {functionPair("backjobs", &staticPrintBGJobs)},
        {functionPair("frontjob", &staticBringJobToFG)},
        {functionPair("cond", &staticCondExec)},
        {functionPair("notcond", &staticReverseCondExec)},
        {functionPair("cull", &staticCull)},
        {functionPair("usescript", &staticUsescript)},
        {functionPair("output", &staticOutput)}
    };
    
    backgroundMode = false;
    bgJobCount = 0;
    
    infoMap =
    {
        {std::pair<std::string, std::string>("setshellname", SETSHELLINFO)},
        {std::pair<std::string, std::string>("setterminator", SETDELIMINFO)},
        {std::pair<std::string, std::string>("history", HISTORYINFO)},
        {std::pair<std::string, std::string>("newname", NEWNAMEINFO)},
        {std::pair<std::string, std::string>("savenewnames", SAVEALIASINFO)},
        {std::pair<std::string, std::string>("readnewnames", READALIASINFO)},
        {std::pair<std::string, std::string>("newnames", PRINTALIASINFO)},
        {std::pair<std::string, std::string>("frontjob", FRONTJOBINFO)},
        {std::pair<std::string, std::string>("backjob", BACKJOBINFO)},
        {std::pair<std::string, std::string>("cond", CONDINFO)},
        {std::pair<std::string, std::string>("notcond", CONDINFO)},
        {std::pair<std::string, std::string>("!", REPLACEHISTINFO)}
    };
}

//HELPER FUNCTIONS
//readies the shell for the next line of input
//called after all commands, whether successful or not
//currently only requires resetting the tokenList, since currentLine gets overwritten every time
void Shell::reset()
{
    tokenList.clear();
    backgroundMode = false;
    return;
}

void Shell::staticSetShellName(Shell* s)
{
    s->setShellName();
}

//sets the shell name to the everything after the first token
//throws TOO_FEW_ARGS if there are not at least 2 tokens
//note that shell name is allowed to be multiple words, with spaces
void Shell::setShellName()
{
    if (tokenList.size() < 1)
        throw error(RETURNCODE::TOO_FEW_ARGS);
    else
    {
        std::string newName = tokenList[1];
        for (int i = 2; i < tokenList.size(); i++)
            newName = newName + " " + tokenList[i];
        shellName = newName;
    }
    
    std::ofstream configFile("config.ini", std::ios::trunc);
    configFile << shellName << " " << shellDelimiter;
    configFile.close();
    return;
}

void Shell::staticSetShellDelimiter(Shell* s)
{
    s->setShellDelimiter();
}

//sets delim to the second token
//throws TOO_FEW_ARGS or TOO_MANY_ARGS if there are not exactly two tokens
//unlike shell name, I decided the delimiter should only be one "word"
void Shell::setShellDelimiter()
{
    if (tokenList.size() < 2)
        throw error(RETURNCODE::TOO_FEW_ARGS);
    if (tokenList.size() > 2)
        throw error(RETURNCODE::TOO_MANY_ARGS);
    std::string newDelim = tokenList[1];
    shellDelimiter = newDelim;
    
    std::ofstream configFile("config.ini", std::ios::trunc);
    configFile << shellName << " " << shellDelimiter;
    configFile.close();
    
    return;
}

//replaces "! #" commands with the specified line of history
//throws TOO_FEW_ARGS or TOO_MANY_ARGS if there are not exactly two arguments
//throws INVALID_ARG if stoi function returns an error (meaning it wasn't able to convert the argument to int)
//throws NO_HISTORY if that line of history doesn't exist
void Shell::replaceWithHistory()
{
    //parse everything out
    std::deque<std::string> tempArray;
    tokenizeString(currentLine, &tempArray);
    
    
    if (tempArray.size() < 2)
    {
        throw error(RETURNCODE::TOO_FEW_ARGS);
    }
    else if (tempArray.size() > 2)
    {
        throw error(RETURNCODE::TOO_MANY_ARGS);
    }
    
    int index;
    
    try {
        //can either throw invalid_argument or out_of_range exception
        //I treat both the same
        index = stoi(tempArray[1]);
    }
    catch (std::exception &e)
    {
        throw error(RETURNCODE::INVALID_ARG);
    }
    
    
    //check to see if that line of history exists
    if (historyList.size() < index || index <= 0)
    {
        throw error(RETURNCODE::NO_HISTORY);
    }
    else
    {
        //note index - 1, to compensate between standard counting starting at 1, and the historyList indexing which of course begins at 0
        currentLine = historyList[index - 1];
    }
    return;
}

//increments the job count, creates the job, adds to the job map
void Shell::addJobToBGQueue(std::vector<pid_t> childList)
{
    bgJobCount++;
    bgJob job(bgJobCount, childList, currentLine, time(NULL));
    bgJobQueue.insert(std::pair<int, bgJob>(bgJobCount, job));
    return;
}

//evaluates conditions in three formats:
// ( condition file ) returns 1 if true, -1 if false
// (condition file) returns 2 if true, -2 if false
// condition file returns 3 if true, -3 if false
// throws BAD_SYNTAX if it cannot detect one of these three formats
int Shell::condChecker()
{
    /*each format actually has a different length for the condition section
     format 1 has a minimum of 5:
     cond ( condition file ) //note that this is technically an invalid command, since nothing follows the condition, but we can ignore that here
     formats 2 and 3 have a minimum of 3:
     cond condition file
     or
     cond (condition file)
     */
    
    if (tokenList.size() < 3)
        throw error(RETURNCODE::TOO_FEW_ARGS);
    
    int format;
    struct stat fileStruct;
    std::string condCode;
    if (tokenList[1] == "(")
    {
        if (tokenList.size() < 5)
            throw error(RETURNCODE::TOO_FEW_ARGS);
        
        if (tokenList[4] != ")")
            throw error(RETURNCODE::BAD_SYNTAX);
        
        stat(tokenList[3].c_str(), &fileStruct);
        condCode = tokenList[2];
        format = 1;
    }
    else if (tokenList[1][0] == '(')
    {
        if (tokenList[2].back() != ')')
            throw error(RETURNCODE::BAD_SYNTAX);
        
        std::string file = tokenList[2];
        file.pop_back(); //to delete the ) character from the end
        stat(file.c_str(), &fileStruct);
        condCode = tokenList[1];
        condCode.erase(0, 1); //cutting off the ( character
        format = 2;
    }
    else
    {
        if (tokenList[3] == ")" || tokenList[2].back() == ')')
            throw error(RETURNCODE::BAD_SYNTAX);
        
        stat(tokenList[2].c_str(), &fileStruct);
        condCode = tokenList[1];
        format = 3;
    }
    
    if (condCode == "checke")
    {
        if (! S_ISREG(fileStruct.st_mode))
            format *= -1;
    }
    else if (condCode == "checkd")
    {
        if (! S_ISDIR(fileStruct.st_mode))
            format *= -1;
    }
    else if (condCode == "checkr")
    {
        if ((S_IRUSR & fileStruct.st_mode) != S_IRUSR)
            format *= -1;
    }
    else if (condCode == "checkw")
    {
        if ((S_IWUSR & fileStruct.st_mode) != S_IWUSR)
            format *= -1;
    }
    else if (condCode == "checkx")
    {
        if ((S_IXUSR & fileStruct.st_mode) != S_IXUSR)
            format *= -1;
    }
    else
    {
        throw error(RETURNCODE::INVALID_ARG);
    }
    
    return format;
}

//MAIN PROGRAM FUNCTIONS

//outputs toyshell[1]> (or whatever the values are at the time)
void Shell::printCommandLine()
{
    std::cout << shellName << "[" << commandCount << "]" << shellDelimiter;
    return;
}

//removes leading whitespace from command line
//doesn't remove trailing, because that gets ignored anyway when reading into the deque
//NO LONGER USED, as gcc < 4.9 doesn't know how to handle regex
//i've left it in for now so I can refer back to it later
void Shell::parseCommandLineWhitespace()
{
        currentLine = std::regex_replace(currentLine, std::regex("^\\s* | \\s*$"), std::string(""));
}

//if there is a currently executing script (scriptStack not empty), uses the top of the queue as the current line
//otherwise gets whatever is on the current command line - can be blank
//return value is false if the line was blank or if there was some error reading the line, true otherwise
bool Shell::readCommandLine()
{
    std::cin.clear();
    
    //if a script is executing, the next command will be in [0][0]
    //then we pop this element
    //if the queue at [0] is now empty, we pop it as well
    if (scriptStack.size() != 0)
    {
        NOHISTORYFLAG = true;
        currentLine = scriptStack[0][0];
        std::cout << currentLine << std::endl;  //printing out the command that is going to be executed
        
        scriptStack[0].pop_front();  //remove the command from the queue
    }
    else
        getline(std::cin, currentLine);
    
    //this erases any comments from the string
    //note that if $ is the first non-space character that occurs, the whole string gets erased
    //and the line is treated as though return was hit on a blank line
    size_t comment = currentLine.find_first_of("$");
    if (comment != std::string::npos)
        currentLine.erase(comment);
    
    //check to make sure line isn't just blank spaces
    //cut out everything up to the first non blank character
    //if the line contains no non-blanks, it erases the whole line
    size_t blanks = currentLine.find_first_not_of(" \t\v");
    currentLine.erase(0, blanks);
    //then erases trailing blanks
    blanks = currentLine.find_last_not_of(" \t\v");
    if (blanks != std::string::npos)
        currentLine.erase(blanks+1);
    
    if (currentLine[0] == '!')
        replaceWithHistory();
    //note that the replaceWithHistory command runs immediately - anything beginning with a ! is assumed to be this command and the appropriate history is inserted before continuing
    
    return std::cin.good() && currentLine != "";
}

//splits the given string into words and adds them to the given array
//this code is called both by parseCommandLine which splits the command into the tokenList
//as well as replaceWithHistory, which writes into a temp array
void Shell::tokenizeString(std::string str, std::deque<std::string>* tokenArray)
{
    std::istringstream tokenizer(str);
    tokenizer.clear();  //clears end of file flag, otherwise can't switch to read mode
    std::string temp;
    while (tokenizer >> temp)
    {
        tokenArray->push_back(temp);
    }
    return;
}

//checks for input and output file redirection, as well as piping between commands
void Shell::parseRedirection()
{
    std::deque<std::string> tempArray; //used to delete elements from the tokenList, since erase() invalidates pointers
    tempArray.push_back(tokenList[0]);
    
    childSubCommands.clear();
    std::deque<std::string> tempCmd;
    tempCmd.push_back(tokenList[0]);
    std::string inputFileName = "";
    
    int file;
    bool flag = false; //used to determine if we need to recopy from the tempArray
    
    //since the first token can't be either [ or ], I start at index 1 to avoid potentially empty commands once the file names are removed
    //if the first token is a [ or ], it will give an error when trying to run the command anyway
    for (int i = 1; i < tokenList.size(); i++)
    {
        if (tokenList[i] == "[" || tokenList[i] == "]" || tokenList[i] == "@")
        {
            if (tokenList[i] == "[")
            {
                //next index must exist
                if (tokenList.size() < (i + 2))
                    throw error(RETURNCODE::TOO_FEW_ARGS);
                   
                //open next token for reading and duplicate it into stdin
                inputFileName = tokenList[i+1];
                file = open(inputFileName.c_str(), O_RDONLY);
                if (file == -1)
                    throw error(RETURNCODE::FILE_ERROR);
                dup2(file, STDIN_FILENO);
                close(file);
                i++; //we already handled the next token - the file name
            }
            else if (tokenList[i] == "]")
            {
                if (tokenList.size() < (i + 2))
                    throw error(RETURNCODE::TOO_FEW_ARGS);
                   
                //open next token for writing and duplicate it into stdout
                if (tokenList[i+1] == inputFileName)
                    throw error(RETURNCODE::RECURSIVE_REDIRECTION);
                file = open(tokenList[i+1].c_str(), O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
                if (file == -1)
                    throw error(RETURNCODE::FILE_ERROR);
                dup2(file, STDOUT_FILENO);
                close(file);
                i++;
            }
            else
            {
                childSubCommands.push_back(tempCmd);
                tempCmd.clear();
            }
            flag = true; //we now need to remove at least one token from the list
        }
        else
        {
            //if a file redirection token was not encountered, we copy the current element to the temp array
            tempArray.push_back(tokenList[i]);
            tempCmd.push_back(tokenList[i]);
        }
    }
    childSubCommands.push_back(tempCmd); //push back the last command, it will have been missed in the loop
    
    //after iterating through the original list, we might need to recopy from the temp array in order to delete the [ ] tokens as well as the file names
    if (flag)
    {
        tokenList.clear();
        tokenList.resize(tempArray.size());
        std::copy(tempArray.begin(), tempArray.end(), tokenList.begin());
    }
}

void Shell::parseCommandLine()
{
    //first tokenize the string into token list, which by the nature of my implementation automatically gets rid of leading, trailing, and extra spaces
    tokenizeString(currentLine, &tokenList);

    //leading and trailing spaces are removed from the string version of the command in readCommandLine(), but not extra spaces between words
    //so I recreate the string from the tokens, which allows me to use it later in a few places
    currentLine = tokenList[0];
    for (int i = 1; i < tokenList.size(); i++)
        currentLine = currentLine + " " + tokenList[i];
    
    return;
}

//loops through the input string, assumed to be in tokenList, replaces all aliases, checks for recursive aliasing
//throws RECURSIVE_ALIAS
void Shell::parseAliases()
{
        bool replacementFlag;
        int tokenIndex, aliasIndex;
        
        std::forward_list<std::string> tempArray;  //using a forward list rather than a regular list so I can use insert_after
        std::forward_list<std::string>::iterator tempIndex;
        std::deque<std::string> recursiveAliasCheck;  //alias is recursive if the same alias comes up again in the same location
        //or a location created by the first alias
        
        //we first copy everything from the real token array to a temporary one
        //this allows us to insert multi word aliases into the original location
        //we do the alias checks on this temp array
        //and then copy everything back
        //I'm sure there's a better way to do this but,

        //iterating in reverse order because forward_list only has a push front method, not push back
        for (tokenIndex = (int) tokenList.size() - 1; tokenIndex >= 0; tokenIndex--)
        {
            tempArray.push_front(tokenList[tokenIndex]);
        }
        
        tokenList.clear();
            
        for (tempIndex = tempArray.begin(); tempIndex != tempArray.end(); tempIndex++)
        {
            do
            {
                replacementFlag = false;
                
                for (aliasIndex = 0; aliasIndex < aliasList.size(); aliasIndex++)
                {
                    //if the token currently pointed at matches one of the alias keys, we replace it
                    if (*tempIndex == aliasList[aliasIndex][0])
                    {
                        for (int i = 0; i < recursiveAliasCheck.size(); i++)
                        {
                            //check to make sure that the new value hasn't already come up on this cycle, indicating a circular alias
                            if (aliasList[aliasIndex][1] == recursiveAliasCheck[i])
                                throw error(RETURNCODE::RECURSIVE_ALIAS);
                        }
                        recursiveAliasCheck.push_back(*tempIndex);  //add the alias to the checking queue
                        
                        replacementFlag = true;
                        *tempIndex = aliasList[aliasIndex][1];  //there is guaranteed to be one value, so copy it first
                        //then insert any additional tokens, increasing the size of the list
                        //this is being done in reverse order just like the push_front above, because otherwise I would need to maintain a seperate iterator for the insert location
                        for (int valueIndex = (int) aliasList[aliasIndex].size() - 1; valueIndex >= 2; valueIndex--)
                        {
                            for (int i = 0; i < recursiveAliasCheck.size() - 1; i++)
                            {
                                if (aliasList[aliasIndex][valueIndex] == recursiveAliasCheck[i])
                                    throw error(RETURNCODE::RECURSIVE_ALIAS);
                            }
                            tempArray.insert_after(tempIndex, aliasList[aliasIndex][valueIndex]);
                        }
                    }
                    if (replacementFlag)
                        break; //if we replaced the token on this pass, the while loop is going to cause a restart of the for loop anyway, may as well just break out now and save a couple cycles
                }
            } while (replacementFlag);
            //if a match was found, we need to repeat on the same index, which now contains the first value from the alias list
            //if we did not replace on this pass, we can push the value, whether original token or alias, back to the original array
            tokenList.push_back(*tempIndex);
        }
    return;
}

//adds whatever was on the input line, minus leading and trailing spaces, to the history queue
void Shell::addCommandToHistory()
{
    //delete the first (oldest) element if the queue is full
    if (historyList.size() == maxHistorySize)
        historyList.pop_front();
    historyList.push_back(currentLine);
    return;
}

//selects the right command to run
//assumes the command is already in tokenList
void Shell::execCommand()
{
    commandCount++;
    
    //find the value in the command map
    std::map<std::string, void(*)(Shell*)>::iterator it = commandMap.find(tokenList[0]);
    
    //if the command does not resolve to one of the internal functions, assume it's Linux and let the OS handle it
    if (it == commandMap.end())
        functionPointer = &staticRunLinuxCommand;
    else
        functionPointer = it->second;
    
    
    //calling the static function chosen with the current object, so they can call this->realFunction()
    functionPointer(this);
    
    return;
}

//COMMAND FUNCTIONS

void Shell::staticPrintHistory(Shell* s)
{
    s->printHistory();
}

//prints history queue
//does not throw, since no history isn't an error
void Shell::printHistory()
{
    if (historyList.size() == 0)
        std::cout << "No history to display\n";
    
    for (int i = 0; i < historyList.size(); i++)
    {
        std::cout << i + 1 << ": " << historyList[i] << std::endl;
    }
    return;
}

void Shell::staticPrintAliases(Shell* s)
{
    s->printAliases();
}

//prints aliases
//does not throw, since no aliases isn't an error
void Shell::printAliases()
{
    if (aliasList.size() == 0)
        std::cout << "No aliases.  Use the newname command to enter a new alias\n";
    else
    {
        for (int i = 0; i < aliasList.size(); i++)
        {
            std::cout << i << ": ";
            //because alias list is a queue of queues, we need a double for loop to print
            for (int j = 0; j < aliasList[i].size(); j++)
                std::cout << aliasList[i][j] << " ";
            std::cout << std::endl;
        }
    }
    return;
}

void Shell::staticNewnameCommand(Shell* s)
{
    s->newnameCommand();
}

//if there are two commands, calls deleteAlias
//if there are more than two, calls addNewAlias
//throw TOO_FEW_ARGS otherwise
void Shell::newnameCommand()
{
    if (tokenList.size() == 2)
        deleteAlias();
    else if (tokenList.size() > 2)
        addNewAlias();
    else
        throw error(RETURNCODE::TOO_FEW_ARGS);
}

void Shell::staticDeleteAlias(Shell* s)
{
    s->deleteAlias();
}

//searches the alias list for the key, assumed to be the second token, and removes the alias if found
//throws NO_ALIAS if not found, including if the alias list is empty
void Shell::deleteAlias()
{
    //no need to check that index 1 exists, that happens in Shell::newnameCommand()
    std::string key = tokenList[1];
    bool deletionFlag = false;
    
    //iterate through each element in aliasList
    //since aliasList is a 2D queue, the key is position [i][0] and the value is position [i][1]

    if (aliasList.size() == 0)
        throw error(RETURNCODE::NO_ALIAS);
    
    //deque::erase() requires an iterator, which is why I use that rather than int i = 0
    //this then requires the use of pointer references for i->at and i->clear
    for (std::deque<std::deque<std::string>>::iterator current = aliasList.begin(); current < aliasList.end(); current++)
    {
        if (current->at(0) == key)
        {
            //because it's a 2D queue, I first call clear on the lower dimension to delete the two strings
            //then erase on the upper dimension to delete the lower level queue
            current->clear();
            aliasList.erase(current);
            //can't be in there more than once, so just break out of the loop and exit
            deletionFlag = true;
            break;
        }
    }
    
    if (!deletionFlag)
        throw error(RETURNCODE::NO_ALIAS);
    
    return;
}

void Shell::staticAddNewAlias(Shell* s)
{
    s->addNewAlias();
}

//add new key value pair to the end of the queue
//aliasList is a 2D queue - a deque where each entry is another deque of variable size depending on how long the alias is
//this probably isn't the most efficient solution, given the requirement to delete from the beginning, add to the end, search and retrieve any mapping, as well as delete from the middle.  Deque does the first two well, Map does the last two, so this feels like a bit of an inelegant solution
//if there wasn't a hard cap of size 10, I would probably have written my own double ended map class
//throws NO_OVERRIDE if the user tries to use an internal function name as an alias name
//throws RECURSIVE_ALIAS if any token in the alias value is the same as the key
void Shell::addNewAlias()
{
    //first check to make sure the new alias isn't a preexisting command
    std::map<std::string, void(*)(Shell*)>::iterator it = commandMap.find(tokenList[1]);
    if (it != commandMap.end())
        throw error(RETURNCODE::NO_OVERRIDE);
    
    //no need to check that these values exist, that was already done in newnameCommand()
    std::string key = tokenList[1];
    std::string value;
    bool duplicate = false;
    
    //if full, deletes the top element first
    if (aliasList.size() == maxAliasSize)
        aliasList.pop_front();
    
    //if the alias already exists, replace the old one
    for (std::deque<std::deque<std::string>>::iterator i = aliasList.begin(); i < aliasList.end(); i++)
    {
        //if the key already exists, check for exact duplicate - if yes then leave it, otherwise delete the old value
        if (i->at(0) == key)
        {
            if (tokenList.size() - 1 == i->size())
            {
                duplicate = true;
                for (int j = 0; j < i->size(); j++)
                {
                    if (tokenList[j + 1] != i->at(j))
                        duplicate = false;
                }
            }
            if (!duplicate)
                aliasList.erase(i);  //this just erases the whole line, as opposed to leaving the key
            //doing so means I can always follow the same process for adding an alias, without having to determine if it's a replacement or not
            //it also means the "new" alias gets pushed to the end of the queue rather than maintaining it's original position
            break;
        }
    }
    
    if (!duplicate)
    {
        //the first element in the queue is the key and everything afterwards is the value
        std::deque<std::string> newAliasPair;
        newAliasPair.push_back(key);
        for (int i = 2; i < tokenList.size(); i++)
        {
            value = tokenList[i];
            //recursion check - if any value of the alias is also the key, it won't be able to resolve
            if (value == key)
                throw error(RETURNCODE::RECURSIVE_ALIAS);
            newAliasPair.push_back(value);
        }
        //then push the entire queue to the full 2D alias list
        aliasList.push_back(newAliasPair);
    }
    return;
}

void Shell::staticSaveNewAliasFile(Shell* s)
{
    s->saveNewAliasFile();
}

//saves the alias list to the file name specified in tokenList[1], creates it if it doesn't exist
//throws TOO_FEW_ARGS or TOO_MANY_ARGS if there aren't exactly two tokens
//throws FILE_ERROR if for some reason it isn't able to open the file fro writing
void Shell::saveNewAliasFile()
{
    if (tokenList.size() < 2)
        throw error(RETURNCODE::TOO_FEW_ARGS);
    if (tokenList.size() > 2)
        throw error(RETURNCODE::TOO_MANY_ARGS);
    
    //opening in trunc mode to overwrite anything there previously
    std::ofstream aliasFile(tokenList[1], std::ios::trunc);
    if (!aliasFile)
        throw error(RETURNCODE::FILE_ERROR);
    
    for (int i = 0; i < aliasList.size(); i++)
    {
        for (int j = 0; j < aliasList[i].size(); j++)
        {
            aliasFile << aliasList[i][j] << " ";
        }
        aliasFile << std::endl;
    }
    
    aliasFile.close();
    return;
}

void Shell::staticReadAliasFile(Shell* s)
{
    s->readAliasFile();
}

//reads all aliases from the file specified in tokenList[1]
//throws TOO_FEW_ARGS or TOO_MANY_ARGS if there are not exactly two tokens
//throws FILE_ERROR if the file does not exist or cannot be opened
//throws BAD_FORMAT if any line of the file does not have at least two tokens (key + value)
void Shell::readAliasFile()
{
    if (tokenList.size() < 2)
        throw error(RETURNCODE::TOO_FEW_ARGS);
    if (tokenList.size() > 2)
        throw error(RETURNCODE::TOO_MANY_ARGS);
    
    std::ifstream aliasFile(tokenList[1]);
    if (!aliasFile)
        throw error(RETURNCODE::FILE_ERROR);
    
    std::string tempLine, tempToken;
    std::istringstream tokenParser;
    
    while (getline(aliasFile, tempLine))
    {
        //since addNewAlias already exists and has this functionality fully built, including error and duplicate checks,
        //this function just pushes the alias to tokenList, which is no longer needed, and lets addNewAlias run normally
        tokenParser.str(tempLine);
        tokenParser.clear();
        
        tokenList.clear();
        tokenList.push_back(""); //addNewAlias expects that the alias starts in the second index
        while (tokenParser >> tempToken)
            tokenList.push_back(tempToken);
        if (tokenList.size() < 3)
            throw error(RETURNCODE::BAD_FORMAT);
        addNewAlias();
    }
    return;
}

void Shell::staticCondExec(Shell* s)
{
    s->condExec();
}

//calls condChecker to evaluate the condition
//returns of 1, 2 and 3 indicate success conditions
//then recreates the tokenList with just the command to run
//and calls execCommand again
//throws TOO_FEW_ARGS if there is not at least one argument to pass to execCommand
void Shell::condExec()
{
    //since condChecker accepts three formats, with and without parentheses, the return value on success is captured
    //1 means we cut out tokens 1 through 5
    //2 and 3 mean we only cut 1 through 3
    int format = condChecker();
    if (format == 1)
    {
        //erase() causes a segfault on undefined ranges, so must explicitly check that sufficient args exist
        if (tokenList.size() < 5)
            throw error(RETURNCODE::TOO_FEW_ARGS);
        
        tokenList.erase(tokenList.begin(), tokenList.begin() + 5);
        execCommand();
    }
    else if (format == 2 || format == 3)
    {
        if (tokenList.size() < 3)
            throw error(RETURNCODE::TOO_FEW_ARGS);
        
        tokenList.erase(tokenList.begin(), tokenList.begin() + 3);
        execCommand();
    }
    return;
}

void Shell::staticReverseCondExec(Shell* s)
{
    s->reverseCondExec();
}

//calls condChecker to evaluate the condition
//returns of -1, -2 and -3 indicate success conditions
//then recreates the tokenList with just the command to run
//and calls execCommand again
//throws TOO_FEW_ARGS if there is not at least one argument to pass to execCommand
void Shell::reverseCondExec()
{
    //since condChecker accepts three formats, with and without parentheses, the return value on success is captured
    //-1 means we cut out tokens 1 through 5
    //-2 and -3 mean we only cut 1 through 3
    int format = condChecker();
    if (format == -1)
    {
        if (tokenList.size() < 5)
            throw error(RETURNCODE::TOO_FEW_ARGS);
        
        tokenList.erase(tokenList.begin(), tokenList.begin() + 5);
        
        execCommand();
    }
    else if (format == -2 || format == -3)
    {
        if (tokenList.size() < 3)
            throw error(RETURNCODE::TOO_FEW_ARGS);
        
        tokenList.erase(tokenList.begin(), tokenList.begin() + 3);
        
        execCommand();
    }
    return;
}

void Shell::runChildProcess(int index)
{
    std::istringstream parser;
    std::string currentDirectory;
    char* pathVar = getenv("PATH");
    
    parser.str(pathVar);
    parser.clear();
    
    while (getline(parser, currentDirectory, ':'))
    {
        std::string fullFileName = currentDirectory + '/' + childSubCommands[index][0];
        if (access(fullFileName.c_str(), X_OK) == 0)
        {
            //argv has to be two larger than the tokenList: 1 for the directory, 1 for null
            //eg "ls -l" becomes "/usr/bin/ls", "ls", "-l", NULL
            char** argv = new char*[childSubCommands[index].size() + 2];
            
            argv[0] = strdup(fullFileName.c_str());
            argv[1] = strdup(childSubCommands[index][0].c_str());  //setting the name for the new process
            
            for (int i = 1; i < childSubCommands[index].size(); i++)
            {
                //i + 1
                argv[i+1] = strdup(childSubCommands[index][i].c_str());
            }
            argv[childSubCommands[index].size() + 1] = NULL;
            execve(argv[0], &argv[1], environ);
            delete [] argv; //if execve failed, I need to explicitly free all memory and exit
            std::exit(0);
        }
    }
    //if it makes it to this point, then the command was not found in any $PATH directory
    std::cerr << "Command not found\n";
    std::exit(0);
    return;
}

void Shell::staticRunLinuxCommand(Shell* s)
{
    s->runLinuxCommand();
}

//passes the input as originally given to the OS
void Shell::runLinuxCommand()
{
    if (tokenList.size() < 1)
        throw error(RETURNCODE::TOO_FEW_ARGS);
        
    parseRedirection(); //first check for redirection or piping
    
    pid_t child;
    std::vector<pid_t> childList;
    int returnValue;
    
    //using a 2D array to hold the file descriptors for the pipes
    std::deque<int*> pipes;
    int* cmdPipe;
    
    for (int i = 0; i < childSubCommands.size(); i++)
    {
        //IF ONLY ONE COMMAND:
        if (childSubCommands.size() == 1)
        {
            child = fork();
            
            if (child == 0)
            {
                runChildProcess(0);
            }
            else
                childList.push_back(child);
        }
        else
        {
            //IF FIRST OF CHAIN
            if (i == 0)
            {
                cmdPipe = new int[2];
                pipe(cmdPipe);
                pipes.push_back(cmdPipe);
                
                child = fork();
                
                if (child == 0)
                {
                    dup2(pipes[0][1], STDOUT_FILENO);
                    close(pipes[0][0]);
                    close(pipes[0][1]);
                    runChildProcess(0);
                }
                else
                    childList.push_back(child);
            }
            //IF LAST OF CHAIN
            else if (i == childSubCommands.size() - 1)
            {
                child = fork();
                
                if (child == 0)
                {
                    dup2(pipes[0][0], STDIN_FILENO);
                    close(pipes[0][0]);
                    close(pipes[0][1]);
                    runChildProcess(i);
                }
                else
                {
                    childList.push_back(child);
                    close(pipes[0][0]);
                    close(pipes[0][1]);
                    pipes.pop_front();
                }
            }
            else
            {
                //IF MIDDLE OF CHAIN
                cmdPipe = new int[2];
                pipe(cmdPipe);
                pipes.push_back(cmdPipe);
                
                child = fork();
                
                if (child == 0)
                {
                    dup2(pipes[0][0], STDIN_FILENO);
                    dup2(pipes[1][1], STDOUT_FILENO);
                    close(pipes[0][0]);
                    close(pipes[0][1]);
                    close(pipes[1][0]);
                    close(pipes[1][1]);
                    runChildProcess(i);
                }
                else
                {
                    childList.push_back(child);
                    
                    //we no longer need the first pipe in the list - close and remove
                    close(pipes[0][0]);
                    close(pipes[0][1]);
                    pipes.pop_front();
                }
            }
        }
    }
    
    if (backgroundMode)
    {
        addJobToBGQueue(childList);
    }
    else
    {
        for (int i = 0; i < childList.size(); i++)
        {
            //this do loop is required when running multiple children
            //depending on the way they are scheduled, we might get returned a false error value: EINTR
            do
            {
                //using waitpid for the specific child, because otherwise it will wait for the first job to end
                //which wouldn't necessarily be this one, as other background jobs might end first
                waitpid(childList[i], &returnValue, 0);
            } while (errno == EINTR);
            if (returnValue != 0)
            {
                throw error(RETURNCODE::PROCESS_ERROR);
            }
        }
    }
    
    return;
}

//I didn't actually implement this yet, partly because it wouldn't have been used anyway since we were given a script to run
//and also because I thought this might make a decent contender for the "add a new function" portion of assignment 2
//the intention is for the "man" command to print usage info for all internal functions
//if the argument isn't an internal function, it passes it onto the OS as usual

void Shell::staticInfoCommand(Shell* s)
{
    s->infoCommand();
}

//print usage info for the given command
//if it's not one of my commands, pass it on to linux
void Shell::infoCommand()
{
    if (tokenList.size() < 2)
        throw error(RETURNCODE::TOO_FEW_ARGS);
    
    //if the second command maps to an internal command, found in infoMap, then it prints the value
    //otherwise it runs a regular linux command and lets the real man function handle it
    std::map<std::string, std::string>::iterator it = infoMap.find(tokenList[1]);
    if (it != infoMap.end())
    {
        std::cout << "======================================\n";
        std::cout << it->second;
        std::cout << "======================================\n";
    }
    else
        runLinuxCommand();
    return;
}

//main driver
//calls the other functions in order
//does not throw any exceptions
void Shell::run()
{
    try
    {
        //while command line is empty, print name, counter, delim
        //then read command line
        printCommandLine();
        while (!readCommandLine())
        {
            printCommandLine();
        }
        
        if (!NOHISTORYFLAG)
            addCommandToHistory();
        
        
        //tokenize
        parseCommandLine();
        
        //output commands get interrupted and returned to main
        //would just be a waste of cycles to go through the rest of the process
        //however, since it can be called by other functions (notably cond and notcond), it does exist as a separate function
        if (tokenList[0] == "output")
        {
            output();
        }
        
        //newname is special in that it should not substitute any aliases
        //this allows both the entry of aliases that refer to other aliases that would otherwise get resolved out as well as the deletion of existing aliases
        //it should also not delete the background operator (-)
        if (tokenList[0] != "newname")
        {
            parseAliases();
            
            if (tokenList.back().back() == '-')
                backgroundMode = true;
            
            if (backgroundMode)
            {
                if (tokenList.back() == "-")
                    tokenList.pop_back();
                else
                    tokenList.back().pop_back();
                
                if (tokenList.empty())
                    throw error(RETURNCODE::TOO_FEW_ARGS);
            }
        }
          
        
        //determine command and run it
        execCommand();
        
        if (scriptStack[0].size() == 1)
            scriptStack.pop_front();
    }
    catch (error const &e)
    {
        if (e.errorCode != RETURNCODE::OUTPUT_COMMAND)
        {
            //since I am interpreting the instructions as fully exiting all scripts when any error occurs, this clears the queue and rethrows to main
            //many of these functions can produce errors, all of these get passed back to main and handled there
            scriptStack.clear();
            NOHISTORYFLAG = false;
        }
        throw e;
    }
    
    return;
}

void Shell::staticBringJobToFG(Shell* s)
{
    s->bringJobToFG();
}

void Shell::bringJobToFG()
{
    if (tokenList.size() < 2)
        throw error(RETURNCODE::TOO_FEW_ARGS);
    else if (tokenList.size() > 2)
        throw error(RETURNCODE::TOO_MANY_ARGS);
    
    int id, status;
    std::vector<pid_t> jobPIDList;
    try
    {
        id = stoi(tokenList[1]);
    }
    catch (std::exception &e)
    {
        //if the second arg is not an integer, stoi will throw an exception
        throw error(RETURNCODE::INVALID_ARG);
    }
    
    //try to find the job in the map
    //if it exists, get the list of pids and wait for each
    //otherwise throw an exception
    std::map<int, bgJob>::iterator it = bgJobQueue.find(id);
    if (it == bgJobQueue.end())
        throw error(RETURNCODE::NO_JOB);
    
    jobPIDList = it->second.pidList;
    bgJobQueue.erase(it);
    for (int i = 0; i < jobPIDList.size(); i++)
        waitpid(jobPIDList[i], &status, 0);
}

void Shell::staticPrintBGJobs(Shell* s)
{
    s->printBGJobs();
}

//prints the Job ID, PID for the first child process in the list (since piped commands might have more than one), the original command, status for the last process in the list and start time
//throws TOO_MANY_ARGS if there is not exactly one argument (the backjobs command itself)
void Shell::printBGJobs()
{
    if (tokenList.size() > 1)
        throw error(RETURNCODE::TOO_MANY_ARGS);
    
    std::vector<int> markForDeletion;
    
    int status;
    std::string statString;
    pid_t lastPID;  //used to check the status for the final process in the list
    std::vector<pid_t> PIDList;
    std::cout << "Job | PID |  Command  | Status |   Start Time\n";
    for (std::map<int, bgJob>::iterator i = bgJobQueue.begin(); i != bgJobQueue.end(); i++)
    {
        bgJob cur = i->second;
        PIDList = cur.pidList;
        lastPID = waitpid(PIDList[PIDList.size() - 1], &status, WNOHANG);
        if (lastPID == PIDList[PIDList.size() - 1] || lastPID == -1)
        {
            statString = "Stopped";
            markForDeletion.push_back(i->first);
        }
        else
            statString = "Running";
        std::cout << std::setw(3) << cur.jobID << std::setw(7) << PIDList[0] << std::setw(10) << cur.cmd << std::setw(11) << statString << std::setw(28) << ctime(&cur.startTime); //ctime ends with \n, so no endl here
    }
    
    //I didn't want to deal with erasing values while running iterators, so I add all stopped jobs to a vector, and erase them after
    for (int i = 0; i < markForDeletion.size(); i++)
    {
        std::map<int, bgJob>::iterator it = bgJobQueue.find(markForDeletion[i]);
        bgJobQueue.erase(it);
    }
    
}

void Shell::staticCull(Shell* s)
{
    s->cull();
}

//kill the process (which might actually be a list of processes) specified by the internal Job ID
//requires exactly two tokens, first is the cull command itself, second is an integer
//throws an error if there are not exactly two tokens, or if the second token cannot be resolved to int
//also throws NO_JOB if the job ID is not found in the map
void Shell::cull()
{
    if (tokenList.size() < 2)
        throw error(RETURNCODE::TOO_FEW_ARGS);
    else if (tokenList.size() > 2)
        throw error(RETURNCODE::TOO_MANY_ARGS);
    
    int jobID;
    try
    {
        jobID = stoi(tokenList[1]);
    }
    catch (std::exception &e)
    {
        //if the second arg is not an integer, stoi will throw an exception
        throw error(RETURNCODE::INVALID_ARG);
    }
    
    //try to find the job in the map
    //if it exists, get the list of pids and kill each
    //otherwise throw an exception
    std::map<int, bgJob>::iterator it = bgJobQueue.find(jobID);
    if (it == bgJobQueue.end())
        throw error(RETURNCODE::NO_JOB);
    
    std::vector<pid_t> jobList = it->second.pidList;
    std::string systemCMD;
    //iterating in reverse order bc killing the first process allows the second to proceed immediately
    for (int i = (int) jobList.size() - 1; i >=0; i--)
    {
        systemCMD = "kill " + std::to_string((int)jobList[i]);
        system(systemCMD.c_str());
    }
    return;
}

void Shell::staticUsescript(Shell* s)
{
    s->usescript();
}

//adds a new entry to the member variable scriptStack (a 2D deque)
//this new entry will itself be a queue of the commands in the file, followed by the file name
//so queue[i][0 - (size -2)] are the commands and queue[i][size - 1] is the name, used to check for recursion
void Shell::usescript()
{
    if (tokenList.size() < 1)
        throw error(RETURNCODE::TOO_FEW_ARGS);
    if (tokenList.size() > 2)
        throw error(RETURNCODE::TOO_MANY_ARGS);
    
    std::string scriptname = tokenList[1];
    //first check to see if the new script is already in the script queue
    //if it is, the script is recursive (this works for both self-recursion or mutual-recursion)
    for (int i = 0; i < scriptStack.size(); i++)
    {
        if (scriptname == scriptStack[i][scriptStack[i].size() - 1])
        {
            throw error(RETURNCODE::RECURSIVE_SCRIPT);
        }
    }
    
    std::ifstream file(scriptname);
    if (!file)
        throw error (RETURNCODE::FILE_ERROR);
    
    std::deque<std::string> newScript;
    std::string command;
    while (getline(file, command))
    {
        newScript.push_back(command);
    }
    newScript.push_back(scriptname); //adding the script name itself to the queue
    scriptStack.push_front(newScript);
    
    file.close();
    return;
}

void Shell::staticOutput(Shell* s)
{
    s->output();
}

void Shell::output()
{
    commandCount++;  //since this command never makes it to execCommand(), I manually increment the count here
    for (int i = 1; i < tokenList.size(); i++)
        std::cout << tokenList[i] << " ";
    std::cout << std::endl;
    throw error(RETURNCODE::OUTPUT_COMMAND);  //technically not an error, but works to return control back to main
}

void Shell::staticExit(Shell* s)
{
    s->exit();
}

//tells the program to exit
void Shell::exit()
{
    //technically not an error, but this is a handy way to tell main to exit using existing functionality
    throw error(RETURNCODE::EXIT);
}

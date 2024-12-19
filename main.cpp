#include <csignal>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <sys/resource.h>
#include <sys/wait.h>
#include <sys/ioctl.h>
#include <unistd.h>

enum class CommandError
{
    OK,
    INVALID_ARGUMENT_NUMBER,
    INVALID_ARGUMENT,
    UNKNOWN_COMMAND,
    INVALID_FILE_PATH,
    UNABLE_TO_OPEN_NOTEPAD,
    FORK_ERROR,
    INVALID_PROCESS_INPUT,
    INVALID_PID
};
void eraseLine();
void printKitten(std::string command);
void printKill(std::string command);
void tokensToArgv(const std::vector<std::string> &tokens, char **argv);
std::string getTextColor(std::filesystem::file_status status);
CommandError createProcesses(const std::vector<std::string> &tokens);
CommandError createProcess(const std::vector<std::string> &tokens, int priority = 0);
CommandError listDirContent(const std::vector<std::string> &arguments);
CommandError printFileContents(const std::vector<std::string> &arguments);
CommandError openNotepad(const std::vector<std::string> &arguments);
CommandError executeCommand(const std::vector<std::string> &tokens);
CommandError killCommand(const std::vector<std::string> &arguments);
CommandError killAllCommand(const std::vector<std::string> &arguments);
CommandError niceCommand(const std::vector<std::string> &arguments);
CommandError showPids(const std::vector<std::string> &arguments);
std::string getErrorMessage(CommandError e);
void closeTerminal(int sig);

using TerminalCommand = CommandError (*)(const std::vector<std::string> &);
const std::string cursor = "\033[35m☿☿☿ \033[0m";
const std::unordered_map<std::string, TerminalCommand> terminalCommands = {
    {"ls", listDirContent}, 
    {"cat", printFileContents}, 
    {"notepad", openNotepad}, 
    {"kill", killCommand}, 
    {"killall", killAllCommand}, 
    {"pids", showPids}, 
    {"nice", niceCommand}};
std::unordered_set<int> pids;

std::vector<std::string> splitStringBySpace(const std::string &inputString);

int main()
{
    signal(SIGINT, closeTerminal);
    while (true)
    {
        std::cout << cursor;
        std::string inputBuffer;
        std::getline(std::cin, inputBuffer);
        if (inputBuffer.empty())
            continue;
        std::vector<std::string> tokens = splitStringBySpace(inputBuffer);
        CommandError e = executeCommand(tokens);
        if (e == CommandError::UNKNOWN_COMMAND)
            e = createProcesses(tokens);
        if (e != CommandError::OK)
            std::cout << "\033[31m" << getErrorMessage(e) << "\033[0m" << std::endl;
    }
}

std::vector<std::string> splitStringBySpace(const std::string &inputString)
{
    std::istringstream iss(inputString);
    std::vector<std::string> substrings;
    std::string substring;
    while (std::getline(iss, substring, ' '))
        substrings.push_back(substring);
    return substrings;
}

CommandError executeCommand(const std::vector<std::string> &tokens)
{
    std::string command = tokens[0];
    std::vector<std::string> arguments(tokens.begin() + 1, tokens.end());
    CommandError e;
    try
    {
        e = terminalCommands.at(command)(arguments);
    }
    catch (const std::out_of_range &e)
    {
        return CommandError::UNKNOWN_COMMAND;
    }
    return e;
}

CommandError killCommand(const std::vector<std::string> &arguments)
{
    if (arguments.size() != 1)
        return CommandError::INVALID_ARGUMENT_NUMBER;
    int pid = std::stoi(arguments[0]);
    if (!pids.contains(pid))
        return CommandError::INVALID_PID;
    eraseLine();
    printKill("kill " + arguments[0]);
    kill(pid, SIGKILL);
    pids.erase(pid);
    return CommandError::OK;
}

CommandError killAllCommand(const std::vector<std::string> &arguments)
{
    if (arguments.size() != 0)
        return CommandError::INVALID_ARGUMENT_NUMBER;
    eraseLine();
    printKill("killall");
    for (auto pid : pids)
        kill(pid, SIGKILL);
    pids.clear();
    return CommandError::OK;
}

CommandError niceCommand(const std::vector<std::string> &arguments)
{
    if (arguments.size() != 2)
        return CommandError::INVALID_ARGUMENT_NUMBER;
    int prio = std::stoi(arguments[0]);
    std::string command = arguments[1];
    createProcess({command}, prio);
    return CommandError::OK;
}

CommandError showPids(const std::vector<std::string> &arguments)
{
    if (arguments.size() != 0)
        return CommandError::INVALID_ARGUMENT_NUMBER;
    std::cout << "PIDS:\t";
    for (auto pid : pids)
    {
        std::cout << pid << '\t';
    }
    std::cout << std::endl;
    return CommandError::OK; 
}

CommandError listDirContent(const std::vector<std::string> &arguments)
{
    if (arguments.size() != 0)
        return CommandError::INVALID_ARGUMENT_NUMBER;
    for (const auto &entry : std::filesystem::directory_iterator(std::filesystem::current_path()))
    {
        std::string textColor = getTextColor(entry.status());
        std::cout << textColor << entry.path().filename().string() << "\033[0m\t";
    }
    std::cout << std::endl;
    return CommandError::OK;
}

std::string getTextColor(std::filesystem::file_status status)
{
    switch (status.type())
    {
    case std::filesystem::file_type::regular:
        return "";
    case std::filesystem::file_type::directory:
        return "\033[34m";
    default:
        return "\033[31m";
    }
}

CommandError printFileContents(const std::vector<std::string> &arguments)
{
    if (arguments.size() != 1)
        return CommandError::INVALID_ARGUMENT_NUMBER;
    std::filesystem::path path(arguments[0]);
    if (!std::filesystem::is_regular_file(path))
        return CommandError::INVALID_FILE_PATH;
    std::ifstream fin(path);
    if (!fin)
        return CommandError::INVALID_FILE_PATH;
    eraseLine();
    printKitten("cat " + arguments[0]);
    std::cout << fin.rdbuf();
    std::cout << std::endl;
    return CommandError::OK;
}

CommandError openNotepad(const std::vector<std::string> &arguments)
{
    pid_t pid = fork();
    int status;
    if (pid == -1)
        return CommandError::UNABLE_TO_OPEN_NOTEPAD;
    if (pid == 0)
    {
        execlp("notepad.exe", "notepad.exe", NULL);
        return CommandError::UNABLE_TO_OPEN_NOTEPAD;
    }
    else
    {
        std::cout << "Opened notepad with PID:\t" << pid << std::endl;
        pids.insert(pid);
    }
    return CommandError::OK;
}

CommandError createProcesses(const std::vector<std::string> &tokens)
{
    std::vector<std::string> currentTokens;
    CommandError e = CommandError::OK;
    for (const auto &arg : tokens)
    {
        if (arg == "&&")
        {
            e = createProcess(currentTokens);
            currentTokens.clear();
        }
        else
            currentTokens.emplace_back(arg);
        if (e != CommandError::OK)
            return e;
    }
    e = createProcess(currentTokens);
    return e;
}

CommandError createProcess(const std::vector<std::string> &tokens, int priority)
{
    pid_t pid;
    int status;
    char **argv = new char*[tokens.size() + 1];
    tokensToArgv(tokens, argv);
    pid = fork();
    if (pid < 0)
        return CommandError::FORK_ERROR;
    if (pid == 0)
    {
        execvp(argv[0], argv);
        return CommandError::INVALID_PROCESS_INPUT;
    }
    else
    {
        pids.insert(pid);
        setpriority(PRIO_PROCESS, pid, priority);
        return CommandError::OK;
    }
    return CommandError::OK;
}

void tokensToArgv(const std::vector<std::string> &tokens, char **argv)
{
    for (int i = 0; i < tokens.size(); ++i)
        argv[i] = const_cast<char*>(tokens[i].c_str());
    argv[tokens.size()] = nullptr;
}

void eraseLine()
{
    std::cout << "\x1b[2K";
    std::cout << "\x1b[1A";
}

void printKitten(std::string command)
{
    std::cout << "\033[36m･ω･\033[0m " << command << std::endl;
}

void printKill(std::string command)
{
    std::cout << "\033[31m🜏🜏🜏 " << command << "\033[0m" << std::endl;
}

std::string getErrorMessage(CommandError e)
{
    switch (e)
    {
    case CommandError::INVALID_ARGUMENT_NUMBER:
        return "Неверное число аргументов";
        break;
    case CommandError::INVALID_FILE_PATH:
        return "Файл не найден";
        break;
    case CommandError::UNABLE_TO_OPEN_NOTEPAD:
        return "Не получилось открыть блокнот";
        break;
    case CommandError::FORK_ERROR:
        return "Ошибка создания процесса";
        break;
    case CommandError::INVALID_PROCESS_INPUT:
        return "Неверное имя при создании процесса";
        break;
    case CommandError::INVALID_PID:
        return "Неверный PID";
        break;
    }
    return "";
}

void closeTerminal(int sig)
{
    killAllCommand({});
    struct winsize w;
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);

    std::cout << "\033[31m";
    std::cout << std::endl;
    for (int i = 0; i < w.ws_row / 2; ++i)
    {
        for (int j = 0; j < w.ws_col / 2; ++j)
            std::cout << "🜏 ";
        std::cout << std::endl << std::endl;
    }
    const std::vector<std::string> goodbye = {"FUN BUG FACT:", "ONE DAY YOU'LL HAVE TO ANSWER FOR YOUR SINS", "AND GOD MAY NOT BE SO", "M E R C I F U L"};
    for (int i = 0; i < goodbye.size(); ++i)
    {
        for (int j = 0; j < w.ws_col / 2 - goodbye[i].size() / 2; ++j)
            std::cout << ' ';
        std::cout << goodbye[i] << std::endl;
        sleep(2);
    }
    for (int i = 0; i < w.ws_row / 4 - 1; ++i)
    {
        for (int j = 0; j < w.ws_col / 2; ++j)
            std::cout << "🜏 ";
        std::cout << std::endl << std::endl;
    }
    sleep(2);
    std::cout << "\033[0m" << std::endl;
    exit(666);
}

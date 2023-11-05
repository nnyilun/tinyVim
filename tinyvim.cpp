#include <fstream>
#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <thread>
#include <functional>
#include <cstring>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netdb.h>
#include <ncurses.h>

class myWindow{
public:
    using text_datas = std::vector<std::string>;
    typedef enum{uCOMMON, uCOMMAND, uINSERT} uMode;
    std::map<int, std::string> Mode;
    std::map<std::string, std::function<void()>> commands_pool;

private:
    const int command_size = 30;
    int cur_row, cur_col, row, col, text_row, text_col, page;
    text_datas text;
    text_datas backup_text;
    uMode preMode;
    std::string file;
    bool isRun;
    std::string reg;

    // TODO
    using coordinate = std::pair<int, int>;
    using operation = std::pair<coordinate, std::string>;
    std::vector<operation> operations;
    std::vector<std::pair<int, int>> ops;       // op, page

    std::vector<operation> Roperations;
    std::vector<std::pair<int, int>> Rops;
    void Undo(){
        Roperations.push_back(*(operations.end() - 1));
        Rops.push_back(*(ops.end() - 1));
        if((*(ops.end() - 1)).first == -1)text[(*(ops.end() - 1)).second + (*(operations.end() - 1)).first.first].insert((*(operations.end() - 1)).first.second, (*(operations.end() - 1)).second);
        else{
            
        }
    }
    void Redo(){

    }


    struct server{
        std::string serverAddr;
        bool isConnected;
        const int server_IP_size = 15;
        const int server_port_size = 8;
        struct addrinfo hints;
        struct addrinfo *serverinfo;
        int server_FD;
    }serverInfo;
    

public:
    myWindow() = delete;
    myWindow(const std::string &_file);
    virtual ~myWindow();

    void run();
    void CommonMode();
    void CommandMode();
    void InsertMode();

    void show(const text_datas &datas);
    void showMode(const uMode &t);
    void showXY(const int &x, const int &y);
    void showOperation(const char &op);

    void c_save();
    void c_quit();
    void c_save_and_quit();
    void c_share(const std::string &com);

    void deleteCommand(int op, int _times);
    void pasteCommand(int _times);
    void wJump(int _times);
    void bJump(int _times);
};

myWindow::myWindow(const std::string &_file){
    initscr();
    raw();
    noecho();
    use_default_colors();
    start_color();
    keypad(stdscr,1);
    getmaxyx(stdscr, row, col);

    cur_row = cur_col = text_col = text_row = 0;
    page = 1;
    reg = "";
    operations.clear();
    serverInfo.isConnected = false;

    file = _file;
    std::ifstream in(file, std::ios::in);
    if(!in.is_open()){
        endwin();
        std::cerr << "can't open " << file << " !" << std::endl;
        exit(-1);
    }
    std::string tmp;
    myWindow::text_datas datas;
    while(getline(in, tmp)){
        datas.push_back(tmp);
    }
    in.close();
    backup_text = text = datas;

    Mode[uCOMMON] = "common";
    Mode[uCOMMAND] = "command";
    Mode[uINSERT] = "insert";
    preMode = uCOMMON;

    commands_pool["w"] = [&](){c_save();};
    commands_pool["q"] = [&](){c_quit();};
    commands_pool["wq"] = [&](){c_save_and_quit();};
}

myWindow::~myWindow(){
    endwin();
    if(serverInfo.isConnected){
        try{
        int status = close(serverInfo.server_FD);
        if(status == -1) throw("close error when destruct server socket!");
        }
        catch(const char *err){
            std::cerr << err;
            printf("-Error NO.%d: %s\n", errno, strerror(errno));
        }
    }
}

void myWindow::run(){
    isRun = true;
    move(0, 0);
    show(text_datas(text.begin() + (page - 1) * (row - 2), text.begin() + page * (row - 2)));
    showMode(uCOMMON);
    move(0, 0);
    getyx(stdscr, cur_row, cur_col);
    CommonMode();
}

void myWindow::CommonMode(){
    int op;
    while(isRun){
        op = getch();
        // if(op == 'q') break;
        switch (op){
        case KEY_UP:
            if(cur_row >= 1){
                cur_row--;
                cur_col = cur_col > text[(page - 1) * (row - 2) + cur_row].size() ? text[(page - 1) * (row - 2) + cur_row].size() : cur_col;
            }
            else if(page > 1){
                page--;
                show(text_datas(text.begin() + (page - 1) * (row - 2) , text.begin() + page * (row - 2) >= text.end() ? text.end() : text.begin() + page * (row - 2)));
                cur_row = row - 2;
                cur_col = cur_col > text[(page - 2) * (row - 2) + cur_row].size() ? text[(page - 2) * (row - 2) + cur_row].size() : cur_col;
            }
            break;
        case KEY_DOWN:
            if(cur_row <= row - 3){
                cur_row++;
                cur_col = cur_col > text[(page - 1) * (row - 2) + cur_row].size() ? text[(page - 1) * (row - 2) + cur_row].size() : cur_col;
                cur_row = (page - 1) * (row - 2) + cur_row >= text.size() ? text.size() - (page - 1) * (row - 2) - 1 : cur_row;
            }
            else if(page * (row - 2) < text.size()){
                page++;
                show(text_datas(text.begin() + (page - 1) * (row - 2) , text.begin() + page * (row - 2) >= text.end() ? text.end() : text.begin() + page * (row - 2)));
                cur_row = 0;
                cur_col = cur_col > text[(page - 1) * (row - 2) + cur_row].size() ? text[(page - 1) * (row - 2) + cur_row].size() : cur_col;
            }
            break;
        case KEY_LEFT:
            if(cur_col >= 1){
                cur_col--;
            }
            break;
        case KEY_RIGHT:
            if(cur_col <= col - 1){
                cur_col++;
                cur_col = cur_col > text[(page - 1) * (row - 2) + cur_row].size() ? text[(page - 1) * (row - 2) + cur_row].size() : cur_col;
            }
            break;
        case ':':
            CommandMode();
            break;
        case 'i':
            InsertMode();
            break;
        case 'w':
            wJump(1);
            break;
        case 'b':
            bJump(1);
            break;
        case 'd':
            mvprintw(row - 1, col - 30, "%c", op);
            move(cur_row, cur_col);
            deleteCommand(0, 1);
            break;
        case 'p':
            pasteCommand(1);
            break;
        case 'u':
            Undo();
            show(text_datas(text.begin() + (page - 1) * (row - 2) , text.begin() + page * (row - 2) >= text.end() ? text.end() : text.begin() + page * (row - 2)));
            break;
        default:
            if(op >= '1' && op <= '9'){
                int times = op - '0';
                mvprintw(row - 1, col - 30, "%d", times);
                op = getch();
                while(op >= '0' && op <= '9'){
                    times = times * 10 + op - '0';
                    mvprintw(row - 1, col - 30, "%d", times);
                    op = getch();
                }
                move(cur_row, cur_col);
                if(op == 'w') wJump(times);
                else if(op == 'b') bJump(times);
                else if(op == 'p') pasteCommand(times);
                else if(op == 'd') deleteCommand(0, times);
            }
            move(cur_row, cur_col);
            break;
        }
        if(op != ':')showOperation(op);
        showMode(uCOMMON);
        mvprintw(row - 1, 0, "               ");
        // showXY(cur_row, cur_col);
        move(cur_row, cur_col);
    }
}

void myWindow::CommandMode(){
    mvprintw(row - 1, 0, ":        ");
    showMode(uCOMMAND);
    move(row - 1, 1);
    char command[command_size] = {};
    echo();
    scanw("%s", command);
    noecho();
    auto result = commands_pool.find(command);
    if(result != commands_pool.end()){
        commands_pool[command]();
        mvprintw(row - 1, 0, "                                          ");
        move(cur_row, cur_col);
        return;
    }
    else if(strcmp(command, "connect") == 0){
        char serverIP[serverInfo.server_IP_size] = {}, serverPort[serverInfo.server_port_size] = {};
        memset(&serverInfo.hints, 0, sizeof(serverInfo.hints)); // make sure the struct is empty
        serverInfo.hints.ai_family = AF_UNSPEC;     // don't care IPv4 or IPv6
        serverInfo.hints.ai_socktype = SOCK_STREAM; // TCP stream sockets
        
        try{
            int status = getaddrinfo(serverIP, serverPort, &serverInfo.hints, &serverInfo.serverinfo);
            if(status != 0) throw(gai_strerror(status));

            struct addrinfo *p = serverInfo.serverinfo;
            for(; p != nullptr; p = p->ai_next){
                serverInfo.server_FD = socket(p->ai_family, p->ai_socktype, p->ai_protocol); 
                if(serverInfo.server_FD < 0)continue;

                if (connect(serverInfo.server_FD, p->ai_addr, p->ai_addrlen) == -1) {
                    close(serverInfo.server_FD);
                    continue;
                }
                break;
            }
            if(p == NULL) throw("can not connect to server!");
            freeaddrinfo(serverInfo.serverinfo);

            // TODO connect
        }
        catch(const char *err){
            mvprintw(row - 1, 0, ":server ip or port error!err:%s", err);
        }
    }
    else if(strcmp(command, "create") == 0){
        if(!serverInfo.isConnected){
            mvprintw(row - 1, 0, ":not connect to server!");
        }
        // TODO
    }
    else if(strcmp(command, "join") == 0){
        if(!serverInfo.isConnected){
            mvprintw(row - 1, 0, ":not connect to server!");
        }
        // TODO
    }
    else if(strcmp(command, "exit") == 0){
        if(!serverInfo.isConnected){
            mvprintw(row - 1, 0, ":not connect to server!");
        }
        // TODO
    }
    else{
        mvprintw(row - 1, 0, ":command does not exist!");
    }
    getch();
    mvprintw(row - 1, 0, "                                          ");
    move(cur_row, cur_col);
}

void myWindow::InsertMode(){
    bool _ = true;
    while(_){
        showMode(uINSERT);
        showXY(cur_row, cur_col);
        move(cur_row, cur_col);
        int op = getch();
        if(op == 27) break;
        switch (op){
        case KEY_UP:
            if(cur_row >= 1){
                cur_row--;
                cur_col = cur_col > text[(page - 1) * (row - 2) + cur_row].size() ? text[(page - 1) * (row - 2) + cur_row].size() : cur_col;
            }
            else if(page > 1){
                page--;
                show(text_datas(text.begin() + (page - 1) * (row - 2) , text.begin() + page * (row - 2) >= text.end() ? text.end() : text.begin() + page * (row - 2)));
                cur_row = row - 2;
                cur_col = cur_col > text[(page - 1) * (row - 2) + cur_row].size() ? text[(page - 1) * (row - 2) + cur_row].size() : cur_col;
            }
            break;
        case KEY_DOWN:
            if(cur_row <= row - 3){
                cur_row++;
                cur_col = cur_col > text[(page - 1) * (row - 2) + cur_row].size() ? text[(page - 1) * (row - 2) + cur_row].size() : cur_col;
                cur_row = (page - 1) * (row - 2) + cur_row >= text.size() ? text.size() - (page - 1) * (row - 2) - 1 : cur_row;
            }
            else if(page * (row - 2) < text.size()){
                page++;
                show(text_datas(text.begin() + (page - 1) * (row - 2) , text.begin() + page * (row - 2) >= text.end() ? text.end() : text.begin() + page * (row - 2)));
                cur_row = 0;
                cur_col = cur_col > text[(page - 1) * (row - 2) + cur_row].size() ? text[(page - 1) * (row - 2) + cur_row].size() : cur_col;
            }
            break;
        case KEY_LEFT:
            if(cur_col >= 1){
                cur_col--;
            }
            break;
        case KEY_RIGHT:
            if(cur_col <= col - 1){
                cur_col++;
                cur_col = cur_col > text[(page - 1) * (row - 2) + cur_row].size() ? text[(page - 1) * (row - 2) + cur_row].size() : cur_col;
            }
            break;
        case 27:    // ESC
            _ = false;
            break;
        case 13:    // Enter
            {
                text.insert(text.begin() + (page - 1) * (row - 2) + cur_row, std::string());
                move(0, 0);
                show(text_datas(text.begin() + (page - 1) * (row - 2) , text.begin() + page * (row - 2) >= text.end() ? text.end() : text.begin() + page * (row - 2)));
                mvprintw(row - 1, col - 30, "%c", op);
                move(++cur_row, cur_col);
            }
            break;
        case KEY_BACKSPACE:     // backspace
            {
                text[(page - 1) * (row - 2) + cur_row].erase(text[(page - 1) * (row - 2) + cur_row].begin() + cur_col - 1);
                move(0, 0);
                show(text_datas(text.begin() + (page - 1) * (row - 2) , text.begin() + page * (row - 2) >= text.end() ? text.end() : text.begin() + page * (row - 2)));
                mvprintw(row - 1, col - 30, "%c", op);
                move(cur_row, --cur_col);
            }
            break;
        default:
            {
                std::string tmp = "";
                tmp += (char)op;
                text[(page - 1) * (row - 2) + cur_row].insert(cur_col, tmp);
                move(0, 0);
                show(text_datas(text.begin() + (page - 1) * (row - 2) , text.begin() + page * (row - 2) >= text.end() ? text.end() : text.begin() + page * (row - 2)));
                mvprintw(row - 1, col - 30, "%c", op);
                move(cur_row, ++cur_col);
            }
            break;
        }
    }
    mvprintw(row - 1, 0, "               ");
}

void myWindow::show(const text_datas &datas){
    clear();
    move(0, 0);
    for(auto &i : datas){
        printw("%s\n", i.c_str());
    }
}

void myWindow::showMode(const uMode &t){
    mvprintw(row - 1, col - 20, "----%s----", Mode[t].c_str());
}

void myWindow::showXY(const int &x, const int &y){
    attron(A_BOLD);
    mvprintw(row - 1, 1, "%d, %d", x, y);
    attroff(A_BOLD);
}

void myWindow::showOperation(const char &op){
    attron(A_ITALIC);
    mvprintw(row - 1, 1, "%c", op);
    attroff(A_ITALIC);
}

void myWindow::c_save(){
    std::ofstream out(file, std::ios::out);
    for(auto &i : text){
        out << i << std::endl;
    }
    out.close();
    backup_text = text;
}

void myWindow::c_quit(){
    if(text != backup_text){
        mvprintw(row - 1, 0, "The file has been changed. Save the file and exit.");
        getch();
        mvprintw(row - 1, 0, "                                                   ");
        showMode(uCOMMON);
        return;
    }
    isRun = false;
}

void myWindow::c_save_and_quit(){
    c_save();
    c_quit();
}

void myWindow::deleteCommand(int op, int _times){
    int times = _times;
    if(op == 0){
        op = getch();
    }
    for(int t = 0; t < times; ++t){
        int result;
        switch (op) {
        case 'd':
            {
                reg = text[(page - 1) * (row - 2) + cur_row];
                text.erase(text.begin() + (page - 1) * (row - 2) + cur_row);
                operations.push_back(operation(coordinate(cur_row, cur_col), reg));
                ops.push_back(std::pair<int, int>(-1, (page - 1) * (row - 2)));
            }
            break;
        case 'w':
            {
                result = text[(page - 1) * (row - 2) + cur_row].find(' ', cur_col);
                if(result != std::string::npos){
                    reg = std::string(text[(page - 1) * (row - 2) + cur_row].begin() + cur_col, text[(page - 1) * (row - 2) + cur_row].begin() + cur_col + result);
                    text[(page - 1) * (row - 2) + cur_row].erase(text[(page - 1) * (row - 2) + cur_row].begin() + cur_col, text[(page - 1) * (row - 2) + cur_row].begin() + cur_col + result);
                }
                else {
                    reg = std::string(text[(page - 1) * (row - 2) + cur_row].begin() + cur_col, text[(page - 1) * (row - 2) + cur_row].end());
                    text[(page - 1) * (row - 2) + cur_row].erase(text[(page - 1) * (row - 2) + cur_row].begin() + cur_col, text[(page - 1) * (row - 2) + cur_row].end());
                }
                operations.push_back(operation(coordinate(cur_row, cur_col), reg));
                ops.push_back(std::pair<int, int>(-1, (page - 1) * (row - 2)));
            }
            break;
        case 'b':
            {
                int i;
                for(i = cur_col; i > 0 && text[(page - 1) * (row - 2) + cur_row][i] != ' '; --i);
                reg = std::string(text[(page - 1) * (row - 2) + cur_row].begin() + i, text[(page - 1) * (row - 2) + cur_row].begin() + cur_col + 1);
                text[(page - 1) * (row - 2) + cur_row].erase(text[(page - 1) * (row - 2) + cur_row].begin() + i, text[(page - 1) * (row - 2) + cur_row].begin() + cur_col + 1);
                operations.push_back(operation(coordinate(cur_row, cur_col), reg));
                ops.push_back(std::pair<int, int>(-1, (page - 1) * (row - 2)));
            }
            break;
        default:
            break;
        }
    }
    move(0, 0);
    show(text_datas(text.begin() + (page - 1) * (row - 2) , text.begin() + page * (row - 2) >= text.end() ? text.end() : text.begin() + page * (row - 2)));
    move(cur_row, cur_col);
}

void myWindow::pasteCommand(int times){
    for(int i = 0; i < times; ++i){
        text[(page - 1) * (row - 2) + cur_row].insert(cur_col, reg);
        operations.push_back(operation(coordinate(cur_row, cur_col), reg));
        ops.push_back(std::pair<int, int>(1, (page - 1) * (row - 2)));
    }
    move(0, 0);
    show(text_datas(text.begin() + (page - 1) * (row - 2) , text.begin() + page * (row - 2) >= text.end() ? text.end() : text.begin() + page * (row - 2)));
    move(cur_row, cur_col);
}

void myWindow::wJump(int _times){
    for(int t = 0; t < _times; ++t){
        if(cur_col == text[(page - 1) * (row - 2) + cur_row].size()){
            move(++cur_row, cur_col = 0);
        }
        else{
            auto result = text[(page - 1) * (row - 2) + cur_row].find(' ', cur_col);
            if(result != std::string::npos){
                move(cur_row, cur_col = result + 1);
            }
            else{
                move(cur_row, cur_col = text[(page - 1) * (row - 2) + cur_row].size());
            }
        }
    }
}

void myWindow::bJump(int _times){
    for(int t = 0; t < _times; ++t){
        if(cur_col == 0){
            --cur_row;
            move(cur_row, cur_col = text[(page - 1) * (row - 2) + cur_row].size());
        }
        else{
            int i;
            for(i = cur_col; i > 0 && text[(page - 1) * (row - 2) + cur_row][i] != ' '; --i);
            move(cur_row, cur_col = i - 1 > 0 ? i - 1 : 0);
        }
    }
}

int main(int argc, char **argv){
    if(argc <= 1) {
        std::cerr << "Usage: " << argv[0] << " <filename>" << std::endl;
        return -1;
    }

    myWindow w_main(argv[1]);
    w_main.run();

    return 0;
}
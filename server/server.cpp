#include <iostream>
#include <cstdlib>
#include <cstring>
#include <string>
#include <cstdio>
#include <vector>
#include <map>
#include <mutex>
#include <thread>
#include <future>
#include <functional>
// #include <exception>
// #include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/select.h>
// #include <sys/wait.h>
#include <arpa/inet.h>
#include <poll.h>
// #include <netinet/in.h>
// #include <csignal>
#include <cerrno>
#include <unistd.h>
#include <netdb.h>
#include "color.hpp"

const int HOSTNAME_MAXLEN = 64;
const int BUF_LEN = 64;
const int USERNAME_LEN = 11;

class Server_Socket{
    private:
        int _protocol;
        int _connections;
        std::string _server;
        std::string _IP;
        struct addrinfo hints;
        struct addrinfo *serverinfo;
        int _server_socket_file_descriptor; 
        char hostName[HOSTNAME_MAXLEN];
        std::vector<int> connected_client_file_descriptor;
        std::map<int, std::string> client_name; 
        std::mutex FD_mutex;
        
    private:
        std::function<bool(char*)> isValue = [&](char* name){
            for(auto &i : client_name){
                if(i.second == std::string(name)) return true;
            }
            return false;
        };
        std::function<bool(std::string, int&)> getFD = [&](std::string name, int &fd){
            for(auto &i : client_name){
                if(i.second == std::string(name)){
                    fd = i.first;
                    return true;
                }
            }
            return false;
        };
        std::function<bool(char*, std::string&, std::string&)> parser = [&](char *msg, std::string &name, std::string &m){
            char tmp[] = "/send ", userName[USERNAME_LEN] = {0}, _[BUF_LEN] = {0};
            if(strncmp(tmp, msg, 6) != 0)return false;
            char *pos1 = msg + 6, *pos2 = pos1 + 1;
            while(*pos2 != '\0' && *pos2 != ' ') ++pos2;
            if(*pos2 == '0')return false;
            strncpy(userName, pos1, pos2 - pos1);
            strcpy(_, pos2 + 1);
            name = std::string(userName);
            m = std::string(_);
            return true;
        };
        void* get_in_addr(struct sockaddr *sa);
        class Session{
            private:
                int chatroom_owner_FD;
                std::vector<int> client_FD;
            
            public:
                Session() = delete;
                Session(int owner_FD);
                virtual ~Session();
                Session(const Session&) = delete;
                Session(Session&&) = delete;

                int run();
        };

    public:
        Server_Socket() = delete;
        Server_Socket(const char *ip, std::string server, int protocol, int connections=64);
        virtual ~Server_Socket();
        Server_Socket(const Server_Socket&) = delete;
        Server_Socket(Server_Socket&&) = delete;

        int Create();
        int createSocket();
        int bindSocket();
        int connectSocket();
        int listenConnection(int queueLen=5);
        int acceptConnection();

        int Send(int Socket_FD, void *data, size_t len);
        int Receive(int Socket_FD, void *data, size_t len);

        int run_poll();
        int run();

        void printIPaddr();
        // getpeername()
        // gethostname()
        // gethostbyname()
};

Server_Socket::Server_Socket(const char *ip, std::string server, int protocol, int connections){
    _IP = ip ? ip : "";
    _server = server;
    _protocol = protocol;
    _connections = connections;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = _protocol;
    hints.ai_flags = AI_PASSIVE;

    try{
        int status = getaddrinfo(ip, _server.c_str(), &hints, &serverinfo);
        if(status != 0) throw(gai_strerror(status));
    }
    catch(const char *err){
        std::cerr << err << std::endl;
        exit(-1);
    }
    // FD_ZERO(&connected_client_file_descriptor);
    std::cout << "<server> "<< FRONT_GREEN << "Construct server socket info successfully!" << RESET_COLOR << std::endl;
    gethostname(hostName, HOSTNAME_MAXLEN);
    std::cout << "<server> " << "local name: " << BOLD << hostName << RESET_COLOR << std::endl;
    printIPaddr();
}

Server_Socket::~Server_Socket(){
    try{
        int status = close(_server_socket_file_descriptor);
        if(status == -1) throw("close error when destruct server socket!");
    }
    catch(const char *err){
        std::cerr << "<server> " << FRONT_RED << err << RESET_COLOR << std::endl;
        printf("-Error NO.%d: %s\n", errno, strerror(errno));
        return;
    }
    std::cout << "<server> " << FRONT_GREEN << "Destruct server socket successfully!" << RESET_COLOR << std::endl;
}

int Server_Socket::Create(){
    std::cout << "<server> " << "start..." <<std::endl;
    try{
        std::cout << "<server> " << FRONT_YELLOW << "choosing available address..." << RESET_COLOR <<std::endl;
        int yes = 1;
        struct addrinfo *p = serverinfo;
        for(; p != nullptr; p = p->ai_next){
            _server_socket_file_descriptor = socket(p->ai_family, p->ai_socktype, p->ai_protocol); 
            if(_server_socket_file_descriptor < 0)continue;
            if(bindSocket() < 0){
                close(_server_socket_file_descriptor);
                continue;
            }
            break;
        }
        freeaddrinfo(serverinfo);
        if(p == NULL) throw("invalid serverinfo!");
        if(listenConnection() != 0) throw("listen error!");
        std::cout << "<server> " << "server listen FD: " << BOLD << _server_socket_file_descriptor << RESET_COLOR << std::endl;
    }
    catch(const char *err){
        std::cerr << "<server> " << FRONT_RED << err << RESET_COLOR << std::endl;
        return -1;
    }
    std::cout << "<server> " << FRONT_GREEN << "create socket successfully!" << RESET_COLOR << std::endl;
    return 0;
}

int Server_Socket::createSocket(){
    try{
        _server_socket_file_descriptor = socket(serverinfo->ai_family, serverinfo->ai_socktype, serverinfo->ai_protocol);
        if(_server_socket_file_descriptor == -1) throw("create socket error!");
    }
    catch(const char *err){
        std::cerr << "<server> " << FRONT_RED << err << RESET_COLOR << std::endl;
        printf("-Error NO.%d: %s\n", errno, strerror(errno));
        return errno;
    }
    std::cout << "<server> " << FRONT_GREEN << "create socket successfully!" << RESET_COLOR << std::endl;
    return 0;
}

void* Server_Socket::get_in_addr(struct sockaddr *sa){
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }
    else return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

int Server_Socket::bindSocket(){
    // bind the special port and IP to the socket
    // int reuse = 1;
    // setsockopt(_server_socket_file_descriptor, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(int));
    try{
        int yes = 1;
        setsockopt(_server_socket_file_descriptor, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));
        int status = bind(_server_socket_file_descriptor, serverinfo->ai_addr, serverinfo->ai_addrlen);
        if(status == -1) throw("bind error!");
    }
    catch(const char *err){
        std::cerr << "<server> " << FRONT_RED << err << RESET_COLOR << std::endl;
        printf("-Error NO.%d: %s\n", errno, strerror(errno));
        return errno;
    }
    std::cout << "<server> " << FRONT_GREEN << "bind successfully!" << RESET_COLOR << std::endl;
    return 0;
}

int Server_Socket::connectSocket(){
    // establish connection
    try{
        int status = connect(_server_socket_file_descriptor, serverinfo->ai_addr, serverinfo->ai_addrlen);
        if(status == -1) throw("connect error!");
    }
    catch(const char *err){
        std::cerr << "<server> " << FRONT_RED << err << RESET_COLOR << std::endl;
        printf("-Error NO.%d: %s\n", errno, strerror(errno));
        return errno;
    }
    std::cout << "<server> " << FRONT_GREEN << "connect successfully!" << RESET_COLOR << std::endl;
    return 0;
}

int Server_Socket::listenConnection(int queueLen){
    // need to call bind() before call listen() so that the server is running on a specific port. 
    /* 
        the sequence of calls is:
        getaddrinfo();
        socket();
        bind();
        listen();
        (accept();)
    */
    try{
        int status = listen(_server_socket_file_descriptor, queueLen);
        if(status == -1) throw("listen error!");
    }
    catch(const char *err){
        std::cerr << "<server> " << FRONT_RED << err << RESET_COLOR << std::endl;
        printf("-Error NO.%d: %s\n", errno, strerror(errno));
        return errno;
    }
    std::cout << "<server> " << FRONT_GREEN << "listen successfully!" << RESET_COLOR << std::endl;
    return 0;
}

int Server_Socket::acceptConnection(){
    std::cout << "<server> " << FRONT_YELLOW << "accepting..." << RESET_COLOR << std::endl;
    try{
        struct sockaddr_storage client_addr;
        socklen_t addr_len = sizeof(client_addr);
        int new_fd = accept(_server_socket_file_descriptor, (struct sockaddr *)&client_addr, &addr_len);
        if(new_fd == -1) throw("accept error!"); 
        char remoteIP[INET6_ADDRSTRLEN] = {0};
        std::cout << "<server> ";
        printf("new connection from %s on socket %d\n",
            inet_ntop(client_addr.ss_family, get_in_addr((struct sockaddr*)&client_addr), remoteIP, INET6_ADDRSTRLEN), new_fd);
        std::lock_guard<std::mutex> lock(FD_mutex);
        connected_client_file_descriptor.push_back(new_fd);
    }
    catch(const char *err){
        std::cerr << "<server> " << FRONT_RED << err << RESET_COLOR << std::endl;
        printf("-Error NO.%d: %s\n", errno, strerror(errno));
        return -1;
    }
    return connected_client_file_descriptor.back();
}

int Server_Socket::Send(int Socket_FD, void *data, size_t len){
    void *_ = data;
    try{
        int status = send(Socket_FD, _, len, 0);
        while(status != 0){
            if(status == -1) throw("send error!");
            _ = static_cast<void*>(static_cast<char*>(data) + status);
            len -= status;
            status = send(Socket_FD, _, len, 0);
        }
    }
    catch(const char *err){
        std::cerr << "<server> " << FRONT_RED << err << RESET_COLOR << std::endl;
        printf("-Error NO.%d: %s\n", errno, strerror(errno));
        return errno;
    }
    // std::cout << "<server> " << FRONT_DARKGREEN << Socket_FD << " <--- " << RESET_COLOR << (char*)data << std::endl;
    return 0;
}

int Server_Socket::Receive(int Socket_FD, void *data, size_t len){
    int status = 0;
    try{
        status = recv(Socket_FD, data, len, 0);
        if(status == -1) throw("receive error!");
    }
    catch(const char *err){
        std::cerr << "<server> " << FRONT_RED << err << RESET_COLOR << std::endl;
        printf("-Error NO.%d: %s\n", errno, strerror(errno));
        return -1;
    }
    std::cout << "<server> " << Socket_FD << FRONT_BLUE << " -> " << RESET_COLOR << (char*)data << std::endl;
    if(status == 0){
        std::cout << "<server> " << FRONT_WHITE 
            << "receive 0 byte data, " << BOLD << Socket_FD << RESET_COLOR << " has closed the connection!" 
            << RESET_COLOR << std::endl;
        return -2;
    }
    return status;
}

int Server_Socket::run_poll(){
    std::cout << "<server> " << FRONT_GREEN << "running poll..." << RESET_COLOR << std::endl;
    int fd_size = connected_client_file_descriptor.size() + 5;
    int fd_length = 0;
    pollfd *pfds = (pollfd*)malloc(sizeof(pollfd) * fd_size);

    pfds[0].fd = _server_socket_file_descriptor;
    pfds[0].events = POLLIN;
    fd_length++;

    for(auto &i : connected_client_file_descriptor){
        pfds[fd_length].fd = i;
        pfds[fd_length].events = POLLIN;
        ++fd_length;
    }

    int new_FD;
    struct sockaddr_storage remoteaddr;
    socklen_t addrlen;
    std::string user_name;
    char buf[BUF_LEN] = {0};
    char msg[BUF_LEN + USERNAME_LEN + 10] = {0};
    int msg_len;
    while(true){
        int poll_count = poll(pfds, fd_length, -1);

        if(poll_count == -1){
            std::cerr << "<server> " << FRONT_RED << "poll error!" << RESET_COLOR << std::endl;
            return -1;
        }
        std::cout << "<server> " << "pollserver: " << poll_count << " connections" << std::endl;
        for(int i = 0; i < fd_length; ++i){
            if(pfds[i].revents & POLLIN){
                if(pfds[i].fd == _server_socket_file_descriptor){
                    addrlen = sizeof(remoteaddr);
                    new_FD = acceptConnection();
                    
                    if(new_FD == -1) continue;

                    if(fd_length == fd_size){
                        fd_size *= 2;
                        pfds = (pollfd*)realloc(pfds, sizeof(pollfd) * fd_size);
                    }

                    pfds[fd_length].fd = new_FD;
                    pfds[fd_length].events = POLLIN;
                    ++fd_length;

                    char register_msg[] = "<server> Please enter your user name:\n";
                    int regmsg_len = strlen(register_msg);
                    Send(new_FD, register_msg, regmsg_len);
                    std::cout << "<server> send register message to " << BOLD << pfds[i].fd << RESET_COLOR << std::endl;
                }
                else if(client_name.find(pfds[i].fd) == client_name.end()){
                    int recvR = Receive(pfds[i].fd, buf, BUF_LEN);
                    if(recvR < 0){
                        int sender_fd = pfds[i].fd;
                        if (recvR == -2) {
                            std::cout << "<server> " << FRONT_YELLOW << "pollserver: socket " << RESET_COLOR
                                << BOLD << sender_fd << RESET_COLOR
                                << FRONT_YELLOW << " hung up" << RESET_COLOR << std::endl;
                        }
                        else if(recvR == -1){
                            std::cout << "<server> " << FRONT_RED << "pollserver: receive error! sender id: " << sender_fd << RESET_COLOR << std::endl; 
                        }
                        close(pfds[i].fd);
                        pfds[i] = pfds[--fd_length];
                        std::lock_guard<std::mutex> lock(FD_mutex);
                        for(auto bg = connected_client_file_descriptor.begin(), ed = connected_client_file_descriptor.end(); bg != ed; ++bg){
                            if(*bg == sender_fd){
                                connected_client_file_descriptor.erase(bg);
                                break;
                            }
                        }
                    }
                    else if(strlen(buf) == 0 || strlen(buf) >= 10 || isValue(buf)){
                        char register_msg[] = "<server> your user name is invalid!\n<server> Please enter again:\n";
                        int regmsg_len = strlen(register_msg);
                        Send(pfds[i].fd, register_msg, regmsg_len);
                        std::cout << "<server> send register message again to " << BOLD << pfds[i].fd << RESET_COLOR << std::endl;
                    }
                    else{
                        client_name.insert(std::pair<int, std::string>(pfds[i].fd, std::string(buf)));
                        char register_msg[] = "<server> register successfully!\n";
                        int regmsg_len = strlen(register_msg);
                        Send(pfds[i].fd, register_msg, regmsg_len);
                        std::cout << "<server> " << BOLD << pfds[i].fd << RESET_COLOR << " register successfully" << std::endl;
                    }
                }
                else{
                    int recvR = Receive(pfds[i].fd, buf, BUF_LEN);
                    int sender_fd = pfds[i].fd;
                    if(recvR < 0){
                        if (recvR == -2) {
                            std::cout << "<server> " << FRONT_YELLOW << "pollserver: socket " << RESET_COLOR
                                << BOLD << sender_fd << RESET_COLOR
                                << FRONT_YELLOW << " hung up" << RESET_COLOR << std::endl;
                        }
                        else if(recvR == -1){
                            std::cout << "<server> " << FRONT_RED << "pollserver: receive error! sender id: " << sender_fd << RESET_COLOR << std::endl; 
                        }
                        close(pfds[i].fd);
                        pfds[i] = pfds[--fd_length];
                        std::lock_guard<std::mutex> lock(FD_mutex);
                        for(auto bg = connected_client_file_descriptor.begin(), ed = connected_client_file_descriptor.end(); bg != ed; ++bg){
                            if(*bg == sender_fd){
                                connected_client_file_descriptor.erase(bg);
                                break;
                            }
                        }
                    }
                    else{
                        std::string toPingUser, toMsg;
                        int to_fd;
                        if(parser(buf, toPingUser, toMsg) && getFD(toPingUser, to_fd) && to_fd != sender_fd){
                            std::string temp = client_name[sender_fd] + FRONT_BLUE + std::string(" -> ") + RESET_COLOR 
                                + std::string(FRONT_PURPLE) +std::string("<private> ") + RESET_COLOR
                                + toMsg + std::string("\n\0");
                            strcpy(msg, temp.c_str());
                            msg_len = strlen(msg);
                            Send(to_fd, msg, msg_len);
                        }
                        else{
                            std::string temp = client_name[sender_fd] + FRONT_BLUE + std::string(" -> ") + RESET_COLOR + std::string(buf) + std::string("\n\0");
                            strcpy(msg, temp.c_str());
                            msg_len = strlen(msg);
                            for(int j = 0; j < fd_length; ++j){
                                if(pfds[j].fd == _server_socket_file_descriptor || pfds[j].fd == sender_fd) continue;
                                Send(pfds[j].fd, msg, msg_len);
                            }
                        }
                    }                    
                }   //recv
            }   // POLLIN
        }
    }

    return 0;
}

int Server_Socket::run(){
    // TODO
    return 0;
}

void Server_Socket::printIPaddr(){
    // serverinfo is the head of a linked list
    std::cout << "<server> " << "--------------------" << std::endl;
    printf("<server> IP addresses for %s%s%s on port %s%s%s:\n", BOLD, _IP.size() ? _IP.c_str() : "INET_ADDR_ANY", RESET_COLOR, BOLD, _server.c_str(), RESET_COLOR);
    for(auto p = serverinfo; p != nullptr; p = p->ai_next) {
        void *addr;
        std::string ipver;
        char ipstr[INET6_ADDRSTRLEN];

        if(p->ai_family == AF_INET){
            struct sockaddr_in *ipv4 = (struct sockaddr_in *)p->ai_addr;
            addr = &(ipv4->sin_addr);
            ipver = std::string(FRONT_PURPLE) + "IPv4" + std::string(RESET_COLOR);
        } 
        else{
            struct sockaddr_in6 *ipv6 = (struct sockaddr_in6 *)p->ai_addr;
            addr = &(ipv6->sin6_addr);
            ipver = std::string(FRONT_DARKGREEN) + "IPv6" + std::string(RESET_COLOR);
        }

        inet_ntop(p->ai_family, addr, ipstr, sizeof(ipstr));
        printf("<server>   %s: %s\n", ipver.c_str(), ipstr);
    }
    std::cout << "<server> " << "--------------------" << std::endl;
}

int main(int argc, char **argv){
    int protocol = SOCK_STREAM;
    char *p = nullptr;
    Server_Socket ttt(p, "10001", protocol, 64);
    ttt.Create() == 0 ? ttt.run_poll() : 0;
    return 0;
}
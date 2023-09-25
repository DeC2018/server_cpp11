#include <iostream>
#include <algorithm>
#include <set>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <fcntl.h>

int iostl(int fd, int fiobio, int *pInt);
int set_nonblock(int fd) {
    int flags;
#if defined(NONBLOCK)
    if(-1 == (flags = fcntl(fd, F_GETFL, 0)))
        flags = 0;
    return fcntl(fd, F_SETFL, flags | 0_NONBLOCK);
#else
    flags = 1;
    int FIOBIO;
    return iostl(fd, FIOBIO, &flags);
#endif
}
int iostl(int fd, int fiobio, int *pInt) {
    return 0;
}

int main(int argc_, char **argv) {
    int MasterSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    std::set<int> SlaveSockets;

    struct sockaddr_in SockAddr;
    SockAddr.sin_family = AF_INET;
    SockAddr.sin_port = htons(12345);
    SockAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    bind(MasterSocket, (struct sockaddr *) (&SockAddr), sizeof(SockAddr));

    set_nonblock(MasterSocket);

    listen(MasterSocket, SOMAXCONN);

    while (true) {
        fd_set Set;
        FD_ZERO(&Set);
        FD_SET(MasterSocket, &Set);
        for (auto Iter = SlaveSockets.begin();
             Iter != SlaveSockets.end();
             Iter++) {
            FD_SET(*Iter, &Set);
        }

        int Max = std::max(MasterSocket,
                           *std::max_element(SlaveSockets.begin(),
                                             SlaveSockets.end()));

        select(Max + 1, &Set, NULL, NULL, NULL);

        for (auto Iter = SlaveSockets.begin();
             Iter != SlaveSockets.end();
             Iter++) {
            if (FD_ISSET(*Iter, &Set)) {
                static char Buffer[1024];
                int RecvSize = recv(*Iter,
                                    Buffer,
                                    1024,
                                    MSG_NOSIGNAL);
                if ((RecvSize == 0) && (errno != EAGAIN)) {
                    shutdown(*Iter, SHUT_RDWR);
                    close(*Iter);
                    SlaveSockets.erase(Iter);
                } else if (RecvSize != 0) {
                    send(*Iter, Buffer,
                         RecvSize, MSG_NOSIGNAL);
                }
            }
        }
        if (FD_ISSET(MasterSocket, &Set)) {
            int SlaveSocket = accept(MasterSocket, 0, 0);
            set_nonblock(SlaveSocket);
            SlaveSockets.insert(SlaveSocket);
        }
    }

    return 0;
}
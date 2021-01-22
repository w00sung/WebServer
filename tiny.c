#include "csapp.h"

void doit(int fd);
void read_requesthdrs(rio_t *rp);
int parse_uri(char *uri, char *filename, char *cgiargs);
void serve_static(int fd, char *filename, int filesize);
void get_filetype(char *filename, char *filetype);
void serve_dynamic(int fd, char *filename, char *cgiargs);
void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg);

int main(int argc, char **argv)
{

    int listenfd, connfd;
    char hostname[MAXLINE], port[MAXLINE];
    socklen_t clientlen;
    struct sockaddr_storage clientaddr;

    /* command-line argument 체크 */
    if (argc != 2)
    {
        fprintf(stderr, "usage: %s <port>\n", argv[0]);
        exit(1);
    }

    listenfd = Open_listenfd(argv[1]);
    /* lisetning file descriptor
        - Open_listenfd : Getaddrinfo(NULL, port, &hints, &listp) 
                            - service port주면서 이 port로 듣고있는(?)
                                address information 구조체 연결 리스트 첫 주소 listp에 설정하기
                          +  
                          socket(p->ai_family, p->ai_socktype, p->ai_protocol) 
                            - Getaddrinfo()로 얻어온 주소 구조체 list들의
                                family & socktype & protocol로 만들기
                                listenfd (socket) 만들기 시도

                          bind(listenfd, p->ai_addr, p->ai_addrlen) 
                            - 커널한테
                            listenfd와 
                            Getaddrinfo()로 얻어온 socket_address를 가진 서버의 socket을
                            연결시켜달라고한다.
                            
                            linked list의 구조체들 돌면서 도전! (모두 연결안될 수도 있음)

                          listen(listenfd, LISTENQ) 
                            - 커널한테
                            listenfd는 서버에서(클라이언트 아니고!) 사용될 것이라고 말하면서
                            == listenfd를 active socket(클라이언트)에서 
                                    listening socket(서버)으로 변경시킨다.
                            -> 클라이언트로 부터 connection request를 accept할 수 있다.
                          
                          + return    
    */

    while (1)
    {
        clientlen = sizeof(clientaddr);

        connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);
        /*connection file descriptor 
            - Accept : OPEN + establish connect + return
                (listenfd에 clientaddr를 가지고, clientlen을 가진
                                                클라이언트의 요청 기다림)
                (SA*) : "Generic" socket address structure (긁어오는 hold 용도)
                            server addr인지, client addr인지 모르니 (?) - 확인필요
        */

        Getnameinfo((SA *)&clientaddr, clientlen,
                    hostname, MAXLINE, port, MAXLINE, 0);
        /* 
                소켓 주소 구조체에 대응되는
                호스트와 서비스 이름을 hostname, port(-> 둘다 버퍼임) 버퍼에 채운다. 
            */

        printf("Accepted connection from (%s %s)\n", hostname, port);
        /* 채워진 버퍼를 출력 */
        doit(connfd);
        Close(connfd);
    }

    return 0;
}

void doit(int fd)
{
    int is_static;
    struct stat sbuf;
    char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
    char filename[MAXLINE], cgiargs[MAXLINE];
    rio_t rio;

    /* Request Line(method +URI + version ) 
                        & 
      Request Headers(header-name : header-data) 
    */
    Rio_readinitb(&rio, fd);
    Rio_readlineb(&rio, buf, MAXLINE);
    printf("Request headers : \n");
    printf("%s", buf);
    sscanf(buf, "%s %s %s", method, uri, version);
    if (strcasecmp(method, "GET"))
    {
        // GET method 이외의 요청은 거부한다!
        clienterror(fd, method, "501", "Not implemented",
                    "Tiny does not implement this method");
        return;
    }
}
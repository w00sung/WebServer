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
    // 여기서 uri 입력받음
    sscanf(buf, "%s %s %s", method, uri, version);
    // GET method 인지 check
    if (strcasecmp(method, "GET"))
    {
        // GET method 이외의 요청은 거부한다!
        clienterror(fd, method, "501", "Not implemented",
                    "Tiny does not implement this method");
        return;
    }

    // GET이면 이어서 Request Headers 이어서 읽기
    read_requesthdrs(&rio);

    /* URI((== filename + optional argument)를 
                        parse(쪼개고 분석) from GET request*/

    // flag : 너 static 이냐 dynamic이냐? -> uri를 parse하면 된다.
    is_static = parse_uri(uri, filename, cgiargs); // 여기서 버퍼를 채우나?

    // exist in Disk ?
    if (stat(filename, &sbuf) < 0)
    {
        clienterror(fd, filename, "404", "Not found",
                    "Tiny couldn't find this file");
        return;
    }

    // for Static contents
    if (is_static)
    {
        /* is Regularfile?       ||     has read Permission?    */
        if (!(S_ISREG(sbuf.st_mode)) || !(S_IRUSR & sbuf.st_mode))
        {
            // 해당 파일이 없어요
            clienterror(fd, filename, "403", "Not found",
                        "Tiny couldn't read this file");
            return;
        }
        // 조건 충족시 serve(나른다 client로)
        serve_static(fd, filename, sbuf.st_size);
    }

    // for Dynamic contents
    else
    {
        /* is Regularfile?       ||      is Excutable?    */
        if (!(S_ISREG(sbuf.st_mode)) || !(S_IXUSR & sbuf.st_mode))
        {
            clienterror(fd, filename, "403", "Forbidden",
                        "Tiny couldn't run the CGI program");
            return;
        }

        // 조건 충족시 serve
        serve_dynamic(fd, filename, cgiargs);
    }
}

// error를 client에게 시각화!
void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg)
{

    char buf[MAXLINE], body[MAXBUF];

    /* Build the HTTP !!response body!! -- HTML */
    sprintf(body, "<html><title>Tiny Error</title>");
    sprintf(body, "%s<body bgcolor="
                  "ffffff"
                  ">\r\n",
            body);
    sprintf(body, "%s%s: %s\r\n", body, errnum, shortmsg);
    sprintf(body, "%s<p>%s: %s\r\n", body, longmsg, cause);
    sprintf(body, "%s<hr><em>The Tiny Web Server</em>\r\n", body);

    /* !Print! the HTTP response */
    sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg);
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Content-type: text/html\r\n");
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Content-length: %d\r\n\r\n", (int)strlen(body));
    Rio_writen(fd, buf, strlen(buf));
    Rio_writen(fd, body, strlen(body));
}

void read_requesthdrs(rio_t *rp)
{
    char buf[MAXLINE];

    Rio_readlineb(rp, buf, MAXLINE);
    // 문자열 비교 : 버퍼에 EOF가 올때까지 진행
    while (strcmp(buf, "\r\n"))
    {
        // 읽고 무시한다.
        Rio_readlineb(rp, buf, MAXLINE);
        printf("%s", buf);
    }
    return;
}

int parse_uri(char *uri, char *filename, char *cgiargs)
{
    /*  is_static = parse_uri(uri, filename, cgiargs); 에서 스임 */
    /* uri는 입력받은 상태 */
    char *ptr;

    /* INCLUDING "cgi-bin" : STATIC!*/
    if (!strstr(uri, "cgi-bin"))
    {
        // static 이니까 CGI argu clear
        strcpy(cgiargs, "");

        // filename -> .\0~~~

        /* cpy는 문자열의 \0 까지 복사한다. */
        strcpy(filename, ".");
        /* cat은 \0부터 append를 시작한다. 끝에 \0을 포함하면서! */
        strcat(filename, uri);

        // 만약 끝이 / 였으면? : /home.html 로 바꿔줄거임
        if (uri[strlen(uri) - 1] == '/')
            strcat(filename, "home.html");

        return 1;
    }

    /* NOT including "cgi-bin" : DYNAMIC!*/
    else
    {
        // ? 있는 곳으로 찾아간다.
        ptr = index(uri, '?');
        if (ptr)
        {
            // cgiargs에 ptr+1 이후 문자열을 복사시킨다.
            strcpy(cgiargs, ptr + 1);
            // uri에서 ? 대신끝에 '\0'
            // 넣어줌으로써 앞에서부터 출력시 끊기게 만든다.(not sure)
            *ptr = '\0';
        }
        else
            strcpy(cgiargs, "");

        strcpy(filename, ".");
        strcat(filename, uri);
        return 0;
    }
}

void serve_static(int fd, char *filename, int filesize)
{
    int srcfd;
    char *srcp, filetype[MAXLINE], buf[MAXBUF];

    /* response header를 client에게 보낸다. */

    get_filetype(filename, filetype);
    sprintf(buf, "HTTP/1.0 200 OK\r\n");
    sprintf(buf, "%sServer: Tiny Web Server\r\n", buf);
    sprintf(buf, "%sConnection: close\r\n", buf);
    sprintf(buf, "%sContent-length: %d\r\n", buf, filesize);
    sprintf(buf, "%sContent-type: %s\r\n\r\n", buf, filetype);
    Rio_writen(fd, buf, strlen(buf));
    printf("Response headers:\n");
    printf("%s", buf);

    /* response body를 client에게 보낸다. */

    // Read-Only 영역 : open 시켜놓고 descriptor 얻어오기
    srcfd = Open(filename, O_RDONLY, 0);
    srcp = Mmap(0, filesize, PROT_READ, MAP_PRIVATE, srcfd, 0);
    /*
     srcfd : 이 파일의 첫 filesize byte를 
            READ-ONLY 가상 메모리 영역에 mapping
     srcp : READ-ONLY 가상메모리 영역의 시작주소
     */
    Close(srcfd);

    // 이제 srcp의 filesize만큼은 mapping 되어 있으니 fd에 복사한다.
    Rio_writen(fd, srcp, filesize);
    // 매핑된 가상메모리 주소 반환
    Munmap(srcp, filesize);
}

/* Tiny Web Server에서 filetype은 5개 중 1개다.*/
void get_filetype(char *filename, char *filetype)
{
    if (strstr(filename, ".html"))
        strcpy(filetype, "text/html");
    else if (strstr(filename, ".gif"))
        strcpy(filetype, "image/gif");
    else if (strstr(filename, ".png"))
        strcpy(filetype, "image/png");
    else if (strstr(filename, ".jpg"))
        strcpy(filetype, "image/jpeg");
    else
        strcpy(filetype, "text/plain");
}

void serve_dynamic(int fd, char *filename, char *cgiargs)
{
    char buf[MAXLINE], *emptylist[] = {NULL};

    /* Return First( 성공 + 서버 정보 ) part of HTTP Response */
    sprintf(buf, "HTTP/1.0 200 OK\r\n");
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Server: Tiny Web Server\r\n");
    Rio_writen(fd, buf, strlen(buf));

    if (Fork() == 0)
    {
        setenv("QUERY_STRING", cgiargs, 1);
        Dup2(fd, STDOUT_FILENO);
        Execve(filename, emptylist, environ);
    }
    /* 
        Fork() 
            - 자식 프로세스 생성
        setenv("QUERY_STRING", cgiargs, 1)

            - 환경변수 세팅
            - cgiargs에 CGI 파일이 run time 중에 접근할 수 있게 세팅!
        
        Dup2(fd, STDOUT_FILENO)

            - 자식 프로세스에서 standard output을 
                    연결된 file descriptor(fd)로 redirect 시킨다.
        
        Execve(filename, emptylist, environ)
            - CGI프로그램을 호출 시켜서 실행시킨다.

            - environ : 얘네는 자식단에서 실행시켰기 때문에
                        열린 파일들과, 환경변수들에 동일한 접근성을 갖는다.

    */

    /* 자식이 terminate하면 reap the child 하려고 기다린다.*/
    Wait(NULL);
}
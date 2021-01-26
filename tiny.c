#include "csapp.h"

void doit(int fd);
void read_requesthdrs(rio_t *rp, int echofd, int *content_length);
int parse_uri(char *uri, char *filename, char *cgiargs);
void serve_static(int fd, char *filename, int filesize, int is_head);
void get_filetype(char *filename, char *filetype);
void serve_dynamic(int fd, char *filename, char *cgiargs, int is_head);
void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg);

int main(int argc, char **argv)
{

    int listenfd, connfd;
    char hostname[MAXLINE], port[MAXLINE];
    socklen_t clientlen;
    struct sockaddr_storage clientaddr;

    /* command-line argument port를 추가해줘 !체크 */
    if (argc != 2)
    {
        fprintf(stderr, "usage: %s <port>\n", argv[0]);
        exit(1);
    }

    // Port를 받아서 진행
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
        printf("I'm Here1\n");
        // 여기서 기다린다.!!!
        connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);
        /*connection file descriptor 
            - Accept : OPEN + establish connect + return
                (listenfd에 clientaddr를 가지고, clientlen을 가진
                                                클라이언트(listenfd를 통해 얻어올 수 있음)의 요청 기다림)
                (SA*) : "Generic" socket address structure (긁어오는 hold 용도)
                            server addr인지, client addr인지 모르니 (?) - 확인필요
        */
        printf("I'm Here2\n");

        Getnameinfo((SA *)&clientaddr, clientlen,
                    hostname, MAXLINE, port, MAXLINE, 0);
        /* 

                소켓 주소 구조체에 대응되는
                호스트와 서비스 이름을 hostname, port(-> 둘다 버퍼임) 버퍼에 채운다. 
            */
        printf("I'm Here3\n");

        printf("Accepted connection from (%s %s)\n", hostname, port);
        /* 채워진 버퍼를 출력 */
        doit(connfd);
        printf("I'm Here4\n");

        Close(connfd);
        printf("I'm Here5\n");
    }

    return 0;
}

void doit(int fd)
{
    int is_static = 0;
    int is_head;
    int content_length = 0;
    struct stat sbuf;
    char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
    char filename[MAXLINE], cgiargs[MAXLINE];
    rio_t rio;

    int echofd;
    echofd = Open("new_file.txt", O_RDWR | O_CREAT | O_TRUNC, S_IRWXU | S_IRGRP | S_IROTH);

    /* Request Line(method +URI + version ) 
                        & 
      Request Headers(header-name : header-data) 
    */

    // 버퍼 초기화, rio에 fd를 연결시켜놨음
    Rio_readinitb(&rio, fd);
    Rio_readlineb(&rio, buf, MAXLINE);
    /* Echo
    Rio_writen(fd,buf,strlen(buf));
    
    Rio_writen : 클라이언트에서 철력기능이 있는 것이아니다.
                Telnet의 경우는 출력이 자동으로 되는거임
    */
    Rio_writen(echofd, buf, strlen(buf));

    printf("Request headers : \n");
    Rio_writen(echofd, "Request headers : \n", strlen("Request headers : \n"));
    printf("%s", buf);
    // 여기서 uri 입력받음
    sscanf(buf, "%s %s %s", method, uri, version);
    // buf(에 받아서) -> %s %s %s -> method, uri, version(로 인자 분리)

    // GET method 인지 (strcasecmp : 대소문자 구분 X)check
    if (strcasecmp(method, "GET") && strcasecmp(method, "HEAD") && strcasecmp(method, "POST"))
    {
        // GET method 이외의 요청은 거부한다!
        clienterror(fd, method, "501", "Not implemented",
                    "Tiny does not implement this method");
        return;
    }

    // HEAD가 아니면 이어서 Request Headers 이어서 읽기
    read_requesthdrs(&rio, echofd, &content_length);
    Close(echofd);

    // head면 1 return
    is_head = !strcasecmp(method, "HEAD");

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

    // POST라면, 이제 content-Length 만큼 cgiargs를 읽어야 한다.
    if (!strcasecmp(method, "POST"))
    {
        // cgiargs에 cgiargs 담기 , content_length : 마지막에 '\0'을 추가해준다.
        Rio_readlineb(&rio, cgiargs, content_length + 1);
    }

    // for Static contents
    if (is_static)
    {
        /* is Regularfile?       ||     has read Permission?    */
        if (!(S_ISREG(sbuf.st_mode)) || !(S_IRUSR & sbuf.st_mode))
        {
            // 해당 파일을 다룰 수 없어요
            clienterror(fd, filename, "403", "Forbidden",
                        "Tiny couldn't read this file");
            return;
        }
        // 조건 충족시 serve(나른다 client로)
        serve_static(fd, filename, sbuf.st_size, is_head);
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
        serve_dynamic(fd, filename, cgiargs, is_head);
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

void read_requesthdrs(rio_t *rp, int echofd, int *content_length)
{
    char buf[MAXLINE];

    // 두번째 header를 버리는 용도
    // 얘는 host의 정보를 출력하지 않음!!!
    Rio_readlineb(rp, buf, MAXLINE);
    Rio_writen(echofd, buf, strlen(buf));
    // Rio_writen(rp->rio_fd, buf, strlen(buf));
    // 문자열 비교 : 버퍼에 EOF가 올때까지 진행
    while (strcmp(buf, "\r\n"))
    {
        // 읽고 무시한다.
        Rio_readlineb(rp, buf, MAXLINE);
        Rio_writen(echofd, buf, strlen(buf));
        if (strstr(buf, "Content-Length"))
        {
            // content-length 있는 위치에 ptr 위치
            char *ptr = index(buf, ':');
            ptr += 2;
            //atoi(str) : '\0' 까지 훑는다!!!!!
            *content_length = atoi(ptr);
        }

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

    // uri는 /를 동반하고 들어온다.
    // ex) 14.100.123.42/video.mp4
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

            // ***uri 복사값으로 쓸건데, end point 지정**
            *ptr = '\0';
        }
        else
            strcpy(cgiargs, "");

        strcpy(filename, ".");
        strcat(filename, uri);
        return 0;
    }
}
// Response Header 정보만 보내준다. (not body)

// filesize는 stat구조체에서 얻어옴
void serve_static(int fd, char *filename, int filesize, int is_head)
{
    int srcfd;
    char *srcp, filetype[MAXLINE], buf[MAXBUF];

    /* response header를 client에게 보낸다. */

    get_filetype(filename, filetype);
    sprintf(buf, "HTTP/1.0 200 OK\r\n");
    sprintf(buf, "%sServer: Tiny Web Server\r\n", buf);
    // %s해서 계속 하는추가해서 덮어쓰기의 느낌 : buf에 쌓기
    sprintf(buf, "%sConnection: close\r\n", buf);
    sprintf(buf, "%sContent-length: %d\r\n", buf, filesize);
    sprintf(buf, "%sContent-type: %s\r\n\r\n", buf, filetype); // buf에 쌓인 것을 전달하기
    Rio_writen(fd, buf, strlen(buf));
    printf("Response headers:\n");
    printf("%s", buf);

    if (is_head)
        return;
    /* response body를 client에게 보낸다. */
    // Read-Only 영역 : open 시켜놓고 descriptor 얻어오기

    srcfd = Open(filename, O_RDONLY, 0);

    /*  Mmap

    srcp = Mmap(0, filesize, PROT_READ, MAP_PRIVATE, srcfd, 0);
    
    */
    /*
     srcfd : 이 파일의 첫 filesize byte를 
            READ-ONLY 가상 메모리 영역에 mapping

     srcp : READ-ONLY 가상메모리 영역의 시작주소

     */

    // malloc
    // srcp에 filename의 이름을 가진 file을 매핑해야한다.
    srcp = (char *)malloc(filesize);
    Rio_readn(srcfd, srcp, filesize);

    Close(srcfd);

    // 이제 srcp의 filesize만큼은 mapping 되어 있으니 fd에 복사한다.
    Rio_writen(fd, srcp, filesize);

    // free for malloca memory
    free(srcp);
    /* Mmap : 매핑된 가상메모리 주소 반환
    Munmap(srcp, filesize);
    */
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
    else if (strstr(filename, ".mp4"))
        strcpy(filetype, "video/mp4");
    else
        strcpy(filetype, "text/plain");
}

void serve_dynamic(int fd, char *filename, char *cgiargs, int is_head)
{
    //끝을 구분하기 위해서 NULL 삽입
    char buf[MAXLINE], *emptylist[] = {NULL};

    /* Return First( 성공 + 서버 정보 ) part of HTTP Response */
    sprintf(buf, "HTTP/1.0 200 OK\r\n");
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Server: Tiny Web Server\r\n");
    Rio_writen(fd, buf, strlen(buf));
    if (is_head)
    {
        sprintf(buf, "Connection: close\r\n");
        Rio_writen(fd, buf, strlen(buf));
        return;
    }

    /* 
    
    부모 프로세스는 Fork의 리턴값이 0 이 아니기 때문에 
    block에 들어올 수 없다.
    자식도 "복사된" : cgiargs를 이용할 수 있다. 인자들을 갖고있다.

    */
    if (Fork() == 0)
    {
        // OS level에 설정하는 환경변수에 cgiargs를 넣는다.
        setenv("QUERY_STRING", cgiargs, 1);
        Dup2(fd, STDOUT_FILENO);
        // 메모리 관점 접근
        Execve(filename, emptylist, environ);
    }
    /* 
        Fork() 
            - 자식 프로세스 생성
        setenv("QUERY_STRING", cgiargs, 1)

            - 환경변수 세팅
            - **이 환경변수는 OS level에서 설정된다.**
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
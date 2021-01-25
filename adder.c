#include "csapp.h"

int main(void)
{
    // 환경변수(인자가 될) 입력받을 buffer 주소
    // p : "&" 위치 찾을 것
    char *buf, *p;

    // 받을 인자들
    //content : client에게 전달할 HTML
    char arg1[MAXLINE], arg2[MAXLINE], content[MAXLINE];
    int n1 = 0, n2 = 0;

    if ((buf = getenv("QUERY_STRING")) != NULL)
    {
        // index : index
        // strchr : 주소를 가리킨다.
        p = strchr(buf, '&');
        *p = '\0';

        buf = strchr(buf, '=');
        p = strchr(p + 1, '=');
        // \0만나기 전까지 복사
        strcpy(arg1, buf + 1);
        // &이후 원래 \0 만나기 전까지 복사
        strcpy(arg2, p + 1);

        // 문자"열"을 숫자로 변환
        n1 = atoi(arg1);
        n2 = atoi(arg2);
    }

    /* Response Body */
    /* sprint(A, "   ") : A버퍼에 "  "를 넣을것이다. */
    sprintf(content, "QUERY_STRING=%s", buf);
    sprintf(content, "Welcome th add.com: ");
    sprintf(content, "%sTHE internet addition portal.\r\n<p>", content);
    sprintf(content, "%sTHE answer is : %d + %d = %d\r\n<p>",
            content, n1, n2, n1 + n2);
    sprintf(content, "%sThanks for visiting!\r\n", content);

    /* HTTP response : Response Header */
    printf("Connection : close\r\n");
    printf("Content-length : %d\r\n", (int)strlen(content));
    //\r\n\r\n 두번 해주는 것은 Header가 끝났음을 알려주는 행위다.
    printf("Content-type: text/html\r\n\r\n");
    printf("%s", content);
    // 표준 출력 stream을 clear해주는
    fflush(stdout);
    exit(0);
}
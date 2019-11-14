#include    <sys/socket.h>
#include    <sys/types.h>
#include    <sys/wait.h>
#include    <sys/stat.h>
#include    <fcntl.h>
#include    <netinet/in.h>
#include    <arpa/inet.h>
#include    <unistd.h>
#include    <stdio.h>
#include    <stdlib.h>
#include    <string.h>

long getFileSize(FILE* fd){
    long cur = ftell(fd);
    fseek(fd, 0, SEEK_END);
    long len = ftell(fd);
    fseek(fd, cur, SEEK_SET);
    return len;
}

void getContentType(char* file_name, char* dest){
    int file_name_len = strlen(file_name);
    
    if(strcmp(&file_name[file_name_len - 5], ".html") == 0)
        strcpy(dest, "text/html");
    else if(strcmp(&file_name[file_name_len - 4], ".css") == 0)
        strcpy(dest, "text/css");
    else if(strcmp(&file_name[file_name_len - 4], ".jpg") == 0 || strcmp(&file_name[file_name_len - 5], ".jpeg") == 0)
        strcpy(dest, "image/jpeg");
    else if(strcmp(&file_name[file_name_len - 4], ".png") == 0)
        strcpy(dest, "image/png");
    else
        strcpy(dest, "text/plain");    // default file type set to plain text
}

void handleUpload(char* request, size_t len){    // special handler for upload.html
    /* get boundary */
    char* start = strstr(request, "boundary=") + 9;
    char* end = strstr(start, "\r\n");
    char* boundary = strndup(start, end - start);
    
    /* find begin boundary and filename*/
    start = strstr(end, boundary);
    start += strlen(boundary);
    start = strstr(start, "filename=\"");
    start += 10;
    end = strstr(start, "\"");
    char* file_name = strndup(start, end - start);
    char* path_name = (char*)malloc(6 + strlen(file_name));
    strcpy(path_name, "save/");
    strcpy(path_name + 5, file_name);
    FILE* save = fopen(path_name, "w");
    
    /* find end boundary and write file content to file */
    start = strstr(end, "\r\n\r\n") + 4;
    end = &request[len - strlen(boundary) - 1];
    while(strncmp(end, boundary, strlen(boundary)) != 0)    // find content end
        end--;
    end--;end--;end--;end--;    // remove newline
    fwrite(start, 1, end - start, save);
    
    fclose(save);
    free(file_name);
    free(path_name);
    free(boundary);
    free(file_name);
}

void handle(int sockfd, char* request, size_t len){
    /* parse request header */
    char type[16];    //request type, like GET, POST...
    if(strncmp(request, "GET", 3) == 0)
        strcpy(type, "GET");
    else if(strncmp(request, "POST", 4) == 0)
        strcpy(type, "POST");
    else{
        write(sockfd, "Cannot parse request...", 23);
    }
    
    char path[1024];    //request path
    char contentType[32];
    char* end = strstr(request + strlen(type) + 2, " ");
    strncpy(path, &request[strlen(type) + 2], end - request - strlen(type) - 2);
    path[end - request - strlen(type) - 2] = '\0';
    if(path[0] == '\0')
        strcpy(path, "index.html");    //show index.html when path is '/'
    getContentType(path, contentType);
    
    if(strcmp(path, "myupload.html") == 0 && strcmp(type, "POST") == 0)
        handleUpload(request, len);
    
    /* read from file */
    FILE* fd = fopen(path, "r");
    if(fd == NULL){
        fprintf(stderr, "file not found: %s\n", path);
        fd = fopen("not_found.html", "r");
        strcpy(contentType, "text/html");
    }
    long filelen = getFileSize(fd);
    char* content = (char*)malloc(filelen);
    fread(content, 1, filelen, fd);
    
    /* write header and content to client socket */
    write(sockfd, "HTTP/1.1 200 OK\r\n", 17);
    char temp[32];
    sprintf(temp, "Content-Type: %s\r\n", contentType);
    write(sockfd, temp, sizeof(temp));
    sprintf(temp, "Content-Length: %ld\r\n\r\n", filelen);
    write(sockfd, temp, strlen(temp));
    write(sockfd, content, filelen);
    
    /* release */
    fclose(fd);
    free(content);
}

int main(){
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in addr;    // the address bind to server socket
    
    /* set addr's info */
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(8787);
    
    if(bind(sockfd, (struct sockaddr*)&addr, sizeof(addr)) != 0){
        fprintf(stderr, "bind() failed\n");
        close(sockfd);
        return -1;
    }
    listen(sockfd, 10);
    
    while(1){
        /* socket for client */
        struct sockaddr_in cliaddr;
        socklen_t cliaddrlen;
        int clisockfd = accept(sockfd, (struct sockaddr*)&cliaddr, &cliaddrlen);
        
        pid_t pid = fork();
        if(pid == 0){
            char* buffer = (char*)malloc(1024 * 1024);    // http request header
            memset(buffer, 0, 1024 * 1024);
            size_t len = read(clisockfd, buffer, 1024 * 1024);
            printf("----------new accept----------\n");
            write(1, buffer, len);
            printf("\n------------------------------\n");
            
            handle(clisockfd, buffer, len);
            //write(clisockfd, "hello", 5);
            
            free(buffer);
            close(clisockfd);
            close(sockfd);
            exit(0);
        }
        else
            waitpid(pid, NULL, 0);
        
        close(clisockfd);
    }
    
    close(sockfd);
    
    return 0;
}


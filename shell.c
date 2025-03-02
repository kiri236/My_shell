#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

#define EXIT_SUCCESS 0
#define EXIT_FAILURE 1
#define LSH_RL_BUFSIZE 1024 // 读入命令的缓冲区大小
#define LSH_TOK_BUFSIZE 64 // 解析命令的缓冲区大小
#define LSH_TOK_DELIM " \t\r\n\a" // 解析命令的分隔符
/* 纯手工写法
char *lsh_read_line(void)
{
    int bufsize = LSH_RL_BUFSIZE;
    int position = 0 ;// 开始读入的位置是0
    char *buffer = malloc(sizeof(char)*bufsize);// 为读入的命令分配内存
    int c;
    if(!buffer)
    {
        fprintf(stderr,"lsh: allocation error(内存分配错误)");//stderr 是 C 标准库中预定义的标准错误输出流，用于输出错误信息。例如，使用 fprintf(stderr, "错误信息\n"); 可以将错误信息输出到标准错误流。
        exit(EXIT_FAILURE);//退出程序
    }
    while(1)
    {
        // 读入一个字符
        c = getchar();

        // 如果读入的是EOF或者换行符,就将其替换为null字符并返回
        if(c== EOF || c == '\n')
        {
            // 将当前位置的字符替换为null字符
            buffer[position] = '\0';
            return buffer;
        }else 
        {
            // 没读完就继续读
            buffer[position]=c;
        }
        position++;// 读完一个字符后,位置后移
        // 如果超出了buffer,就要重新分配内存
        if(position >= bufsize)
        {
            bufsize += LSH_RL_BUFSIZE;
            buffer = realloc(buffer,bufsize);
            if(!buffer)
            {
                fprintf(stderr,"lsh: allocation error(内存分配错误)");
                exit(EXIT_FAILURE);
            }
        }
    }
}
*/
// 使用getline函数读入命令
char *lsh_read_line(void)
{
    char *line = NULL;
    ssize_t bufsize = 0 ;// getline 会自动分配内存
    if(getline(&line , &bufsize,stdin)==-1)
    {
        if(feof(stdin))// 如果文件结束,feof返回的是非0值,否则feof返回0
        {
            exit(EXIT_SUCCESS);
        }else 
        {
            perror("lsh: getline\n");// perror 会将错误信息输出到stderr
            exit(EXIT_FAILURE);
        }
    }
    return line;
}
char **lsh_split_line(char *line)
{
    int bufsize = LSH_TOK_BUFSIZE;
    char **tokens = malloc(bufsize*sizeof(char*));
    char *token;
    int position = 0 ;
    if(!tokens)
    {
        fprintf(stderr,"lsh: allocation error(内存分配错误)\n");
        exit(EXIT_FAILURE);
    }
    token = strtok(line,LSH_TOK_DELIM);// strtok 会将字符串分割成若干个子字符串,返回第一个子字符串的指针
    while(token!=NULL)
    {
        tokens[position++] = token;
        if(position >= bufsize)
        {
            // 如果超出了buffer,就要重新分配内存
            bufsize += LSH_TOK_BUFSIZE;
            tokens = realloc(tokens,bufsize*sizeof(char*));
            if(!tokens)
            {
                fprintf(stderr,"lsh: allocation error(内存分配错误)\n");
                exit(EXIT_FAILURE);
            }
        }
        token = strtok(NULL,LSH_TOK_DELIM);//这里第一个参数NULL的意思是继续上次的分割
    }
    tokens[position] = '\0';
    return tokens;
}
int lsh_launch(char ** args)
{
    pid_t pid,wpid;
    int status;

    pid = fork();
    if(pid==0)
    {
        //处理子进程
        if(execvp(args[0],args)==-1)
        {
            // 如果execvp返回-1,说明出错了
            perror("lsh");
        }
        exit(EXIT_FAILURE);
    }else if(pid<0)
    {
        // 如果fork返回-1,说明出错了
        perror("lsh");
    }else 
    {
        // 处理父进程
        do
        {
            wpid = waitpid(pid,&status,WUNTRACED);// 等待子进程结束
        }while(!WIFEXITED(status)&&!WIFSIGNALED(status));// 如果子进程正常结束或者被信号终止,就结束循环
    }
    return 1;
}
int lsh_cd(char ** args);
int lsh_help(char **args);
int lsh_exit(char **args);
char *builtin_str[]=
{
    "cd",
    "help",
    "exit"
};
int (*builtin_func[])(char **)=
{
    &lsh_cd,
    &lsh_help,
    &lsh_exit
};//一个函数指针数组,其元素都是接受char**类型参数,返回int的函数
int lsh_num_builtins()
{
    return sizeof(builtin_str)/sizeof(char*);
}
//实现各个内建命令
int lsh_cd(char **args)
{
    if(args[1]==NULL)
    {
        fprintf(stderr,"lsh: expected argument to \"cd\"\n");
    }else 
    {
        if(chdir(args[1])!=0)
        {
            perror("lsh");
        }
    }
    return 1;
}
int lsh_help(char ** args)
{
    printf("This is my LSH\n");
    printf("Type program name and arguments,then hit enter\n");
    printf("There are builtin commands:\n");
    for(int i = 0; i < lsh_num_builtins();i++)
    {
        printf("%s\n",builtin_str[i]);
    }
    printf("Use the man command for information on other programs.\n");
    return 1;
}
int lsh_exit(char ** args)
{
    return 0;
}
int lsh_execute(char **args)
{
    int i ;
    if(args[0]==NULL)return 1;
    for(i = 0 ; i < lsh_num_builtins();i++)
    {
        if(strcmp(args[0],builtin_str[i])==0)
        {
            return (*builtin_func[i])(args);
        }
    }
    return lsh_launch(args);
}
void lsh_loop(void)
{
    char *line;// 用于存储读入的命令
    char **args;// 用于存储解析后的命令
    int status;// 用于存储命令执行的状态

    do
    {
        printf(">= ");
        line = lsh_read_line();// 读入命令
        // 读入命令后要解析命令,将命令分割成程序及参数
        args = lsh_split_line(line);
        //最后执行解析后的命令
        status = lsh_execute(args);

        // 执行完了要释放资源
        free(line);
        free(args);
    }while(status);
}
int main(int argc,char **argv)
{
    // 初始化阶段,读入并执行配置文件,此处省略
    // 解释阶段,从标准输入读入命令并执行
    lsh_loop();
    // 退出阶段,释放资源,此处省略
    return EXIT_SUCCESS;
}
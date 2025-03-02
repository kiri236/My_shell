自顶向下设计
# shell的生命周期
shell的一个生命周期有三个阶段
- 初始化:shell读入并执行其配置文件(Q:配置文件哪里配置)
- 解释:然后shell从标准输入读入命令并且执行他们
- 终止:当shell的命令执行完后,shell会执行任意的结束命令,并释放内存,然后结束
具体代码如下:
```c
int main(int argc,char **argv)
{
    // 初始化阶段,读入并执行配置文件,此处省略
    // 解释阶段,从标准输入读入命令并执行
    lsh_loop();
    // 退出阶段,释放资源,此处省略
    return EXIT_SUCCESS;
}
```
# shell中的基本循环
在解释阶段我们一直`loop`来不断读取用户命令,在`loop`中处理命令的三步:
- 读取命令:从标准输入读取命令
- 解析:将命令字符串分为程序及其参数
- 运行:运行解释后的字符串
具体代码如下:
```c
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
```
这段代码首先定义了几个变量,然后使用`do-while`循环读入,解析,执行用户输入的命令,如果`status`为0则跳出循环,`status`的值由`lsh_execute()`来决定
# 读入一行命令
因为不知道用户读入多少字符,所以我们要预先分配一份内存,然后如果不够,再分配多的内存
具体代码如下:
```c
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
```
需要注意的几点
- 每次读入的字符是用`int`存储,因为`EOF`的值也是`int`
- 如果读入遇到了换行或`EOF`我们就停止读入并返回该字符串,如果没有就继续读并将读到的字符加在当前字符串的后面
- 读入每一行一个简单的写法就是使用`getline()`函数,使用`getline()`函数进行重写后
```c
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
```
- `EOF`有两种情况:一种情况是读到文件结尾,另一种是用户以`CTRL+D`结束
# 解析命令
解析命令由`lsh_split_line()`完成,将输入的`line`解析为一系列参数,此处做了一点简化,就是命令中不允许有引号和`Backspace`,只使用空格来分割参数
```c
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
```
思路和`lsh_read_line`大致相似
这里的`strtok`函数是将目标字符串分割后将其第一个指针返回并在返回的每个`token`后面加上一个`\0`
这个操作完成后就得到了一个token数组
# shell开始进程
在`Unix`中有两种开启进程的方式
1. 使用`init`在`Unix`启动时,其内核也载入,当其载入后,就会开始一个进程`init`,然后用`init`载入其他进程
2. 很多进程不会自动启动,那就只有一个方式进行启动:`fork()`。当该函数被调用时,操作系统会复制一份该进程,然后把他们都启动,原始的程序称作`parent`,复制的那一份叫做`child`,`fork()`返回`0`给`child`,然后向`parent`返回`child`的进程ID(`PID`),这个方法有个缺点就是运行进程时只能通过运行现有程序的复制件,这时就需要用`exec()`函数,`exec()`函数,`exec()`函数将当前运行的程序替换为一个新的程序,它不创建新的进程,只是将原来的程序停掉,然后运行一个新程序,一般来讲,`exec()`函数不返回任何值,除非运行出错,才会返回`-1`
那么在`Unix`上运行程序的基本思路是
1. 将已有的进程`fork()`成两个不同的进程
2. 让子进程(`child`)使用`exec()`将它自己替换为另一个进程
3. `parent`可以继续做其他事情,也可以继续监视子进程
```C
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
```
这个程序首先接收一串参数,然后将该进程`fork`一下,并将`fork`的返回值保存至`pid`中,当`fork()`完成的时候,有两个进程在运行:
- 当`pid==0`时对子进程进行处理:
	在子进程中,我们希望运行用户输入的指令,所以使用`exec`的一种变体`execvp`,该函数接收一个程序名和一个字符串数组作为参数`excvp`中的`v`代表接收一个字符串数组作为参数,`p`代表不用给出程序具体路径,只用给出程序名称即可
	如果`exec`返回`-1`那么就出错了
- 当`pid<0`时检查`fork()`有没有出错,出错了就告诉用户,让用户决定是否退出
- 当`pid>0`时意味着`fork()`执行成功,父进程能到这里
	子进程将要执行这个进程,所以父进程需要等待完成运行的指令,所以使用`waitpid()`来等待进程状态改变,进程有很多状态,不是所有状态都代表进程结束,一个进程可以正常退出,也可以被信号杀死,所以我们用特定的宏来等待进程退出或被杀死
- 最后返回`1`说明需要输入下一条指令
# Shell内建命令
大多数命令都是程序,但也有一部分命令是shell的内建命令
举个例子
	如果想改变目录,需要使用函数`chdir()`,但是当前目录是程序独有的,假如要写个`cd`来更改该进程的目录,那么只会更改该进程的目录然后结束,其父进程的目录不会改变,但如果shell自己调用了`chdir()`,那么其子进程的目录也会跟着改变
	类似的,`exit`指令不退出调用它的shell本身,该指令也应该作为内建指令
	大多数shell的配置由配置脚本所决定,这些脚本只改变shell进程内的一些操作
```C
int lsh_cd(char ** args);
int lsh_help(char **args);
int lsh_exit(char **args);
char *builtin_str[]=
{
    "cd",
    "help",
    "exit"
};
int (*builtin_func)(char **)=
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
    for(int i = 0; i < lsh_num_builtins;i++)
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
```
这段代码由三个部分组成
1. 进行函数声明,以便后续使用
2. 接下来定义内建命令名数组,然后定义其对应的函数数组,注意这一句
```C
int (*builtin_func)(char **) =
{
    &lsh_cd,
    &lsh_help,
    &lsh_exit
};
```
这句话定义了一个名叫`builtin_func`的函数指针数组,其中的每个元素都是接收`char**`作为参数,返回`int`的函数的指针
3. 最后将这些函数一一实现 
	- `lsh_cd()`首先查看参数的第二项存不存在,如果不存在就显示错误,然后调用`chdir()`函数检查错误并返回一个值
	- `lsh_help()`显示可用的内建指令
	- `lsh_exit()`直接返回`1`表示退出
# 将内建指令和进程放在一起
```C
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
```
先判断是否是内建命令,如果是就执行内建命令,否则执行进程
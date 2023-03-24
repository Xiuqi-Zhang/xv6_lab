#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"

char*
fmtname(char *path)
{
  //DIRSIZ在fs.h中定义，为14
  //static修饰的局部变量为静态局部变量
  static char buf[DIRSIZ+1];
  char *p;

  // Find first character after last slash.
  // 使指针p指向文件名或子文件名的第一个字母所在的位置
  for(p=path+strlen(path); p >= path && *p != '/'; p--)
    ;
  p++;

  // Return blank-padded name.
  if(strlen(p) >= DIRSIZ)
    return p;
  memmove(buf, p, strlen(p));
  //将文件名保存在buf中
  memset(buf+strlen(p), ' ', DIRSIZ-strlen(p));
  //由于文件名小于DIRSIZ定义的大小，所以在文件名后填充对应数量的空格，确保整齐
  return buf;
}

void
ls(char *path)
{
  char buf[512], *p;
  int fd;
  struct dirent de;
  struct stat st; //文件状态结构在kernel/stat.h中定义

  if((fd = open(path, 0)) < 0){
    //open(path, 0)中的0表示只读，相关宏定义在kernel/fcntl.h中
    //open打开成功返回文件描述符，失败返回-1
    fprintf(2, "ls: cannot open %s\n", path);
    return;
  }

  if(fstat(fd, &st) < 0){
    //fstat()函数：将指定的已打开的文件描述符所指向的文件状态保存到指定的stat结构体中
    //执行成功返回0，失败返回-1
    fprintf(2, "ls: cannot stat %s\n", path);
    close(fd);
    return;
  }

  switch(st.type){
  //判断文件类型
  case T_DEVICE: //设备？
  case T_FILE: //文件
    //如目标是文件，则仅输出该文件的文件名、类型、索引节点、大小
    printf("%s %d %d %l\n", fmtname(path), st.type, st.ino, st.size);
    break;

  case T_DIR: //目录
    if(strlen(path) + 1 + DIRSIZ + 1 > sizeof buf){
      printf("ls: path too long\n");
      break;
    }
    strcpy(buf, path); //将path复制到buf中
    p = buf+strlen(buf); //指针p指向buf的最后一位
    *p++ = '/'; //指针p指向下一位，且下一位的内容为/
    while(read(fd, &de, sizeof(de)) == sizeof(de)){
      //目录文件存储的是一个个dirent结构（记录着索引节点号和文件名）
      //read操作一次读取sizeof(de)大小的内容，即一个dirent结构
      if(de.inum == 0)
        //索引节点0表示无效索引（block 0保存的是super block）
        continue;
      memmove(p, de.name, DIRSIZ);
      //将de.name拼接到p指针后面，也就是在buf后面增加了子文件的文件名
      //所以此时buf中保存的就是到子文件的路径
      p[DIRSIZ] = 0;
      //清空buf尾部后一个位置的内容，确保路径最后没有脏数据干扰
      if(stat(buf, &st) < 0){
        //得到子文件路径后就可以用stat函数确认文件状态
        //stat与fstat的区别：stat接收文件路径，fstat接受文件描述符
        printf("ls: cannot stat %s\n", buf);
        continue;
      }
      //打印该目录下一个子文件的信息
      printf("%s %d %d %d\n", fmtname(buf), st.type, st.ino, st.size);
    }
    break;
  }
  close(fd);
}

int
main(int argc, char *argv[])
{
  int i;

  if(argc < 2){
    ls(".");
    exit(0);
  }
  for(i=1; i<argc; i++)
    ls(argv[i]);
  exit(0);
}

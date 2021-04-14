#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>//ftruncate
#include <fcntl.h>//shm_open
#include <sys/stat.h>//shm_open
#include <sys/mman.h>//shm_open(const char*name,int oflag,mode_t mode)
#include <sys/wait.h>

struct Data{
    int flag, row_idx, col_idx, row_hint, col_hint, row_lower_bound, col_lower_bound, row_upper_bound, col_upper_bound;
};//flag --->決定輪到誰    if col_idx==row_idx--->猜對  col_hint/row_hint--->child process 會先丟flag()      lower_bound/upper bound 會縮小範圍


int main(int argc, char** argv){
    unsigned seed = strtol(argv[1], NULL, 10);//argv[1]--->字串 strtol轉成數字
    const int SIZE = sizeof(struct Data);
    char NAME[] = "/prog1";//shm_open NAME

    int fd = shm_open(NAME, O_CREAT | O_RDWR, S_IRUSR | S_IWUSR);//確定有沒有開share memory
    if(fd == -1){//如果share memory開失敗 fd回傳失敗=-1
        fprintf(stdout, "shm_open error\n");
        exit(EXIT_FAILURE);
    }

    if(ftruncate(fd, SIZE) == -1){//確定share memory size把大小作增減 fd--->file descriptor的size大小 linux file discriptor(indicator/handle os指向答案)
        fprintf(stdout, "ftruncate error\n");//ftrunctate--->fd大小增減
        exit(EXIT_FAILURE);/*宣告好一格share memory 跟physical memory要一個SIZE大的空間*/
    }

    struct Data* data = mmap(NULL, SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);//fork 之 前 要 mmap 的部分,因為map到同一個地方,兩個都各自有一份 
    // data是指向memory的指標 PROT_READ--->映射區域可被讀入 PROT_WRITE--->映射區域可被寫入  MAP_SHARED--->分享mapping官方文件 
     //把share memory還沒映射回原本memory的部分做mmap map回process的memory
    if (data == MAP_FAILED){//kernel 挑map回process哪裡自己做決定 sh_memory 映射回我的memory  fd--->shr_open 從fd offset0(開頭)---->往後size bite 確定映射 ( 對 memory page讀取/寫入 prot_read prot_write )
        fprintf(stdout, "mmap error\n");//map shared 讓另一個proess看的到你寫的東西
         //mmap會回傳 process mapping的指標(struct data)取用addresss space 
        exit(EXIT_FAILURE);//如果mmap失敗
    }
    //開mmap 設定大小 process映射到memory的某個位置,利用指標操縱在memory中的位置 (有memory才能random access位置)
    close(fd);//file descriptor 已經結束 因為已經指好process位置(不用知道share memory在哪裡了 --->可以關掉了)
    //因為前面有設定了data--->起始位置已經取下來了---->可以直接做記憶體的移動 
    data->flag = -2;//flag==-2--->還沒輪到人   父process 子process 決定 輪到誰  ---->flag之值自己假設
    data->row_idx = data->col_idx = -1;
    data->row_hint = data->col_hint = 1;
    data->row_lower_bound = data->col_lower_bound = 0;
    data->row_upper_bound = data->col_upper_bound = 9;
    char buffer[100];//設定buffer陣列用sprintf來存取char來輸出字串

    pid_t pid = fork();//fork之前會把前面宣告的變數都複製一份--->放到data
    //兩個人拿到process pid 父process拿到子process pid  子process拿到0
    if (pid == -1) {//fork失敗
        perror("fork error");//印出error message
        exit(EXIT_FAILURE);
    }
    if (pid == 0) {//產生子process
        srand(seed);//設定一次亂數種子
        int my_pid = getpid();//getpid()可以把現在process id傳出來 
        int row_answer = rand() % 10, col_answer = rand() % 10, quota = 5;
        while(data->flag != 1){//剛開始data->flag==-2進不來迴圈
            if(!data->flag){//data->flag是0進迴圈 parent process 還沒進來
                sprintf(buffer, "[%d Child]: OK\n", my_pid);
                write(1, buffer, strlen(buffer));//1--->輸出 輸出到buffer 以char byte 1次輸出,共輸出strlen(buffer)個byte
                data->flag = 1;//自己跳出迴圈
            }
        }
        //data->flag==1--->給parent process猜完變0  --->再交由child process ---->-1---->遊戲結束 
        while(quota){//quota=0--->結束 每猜一次quota減1
            if(!data->flag){//flag==0才能進入
                if(data->row_idx == row_answer && data->col_idx == col_answer){
                    sprintf(buffer,"[%d Child]: Hit, you win\n", my_pid);
                    write(1, buffer, strlen(buffer));
                    data->flag = -1;//flag設成-1--->猜對(結束) flag--1--->猜錯,繼續
                    break;
                }
                else{
                    sprintf(buffer,"[%d Child]: Miss,", my_pid);//因為沒辦法一次輸出整個句子,使用sprintf--->先存在buffer,最後一次輸出再寫到stdout 

                    if(!--quota){//猜的次數用完了
                        sprintf(buffer + strlen(buffer),"you lose\n");
                        write(1, buffer, strlen(buffer));//1-->stdout buffer--->從buffer輸出 以strlen(buffer)去儲存
                        data->row_idx = row_answer, data->col_idx = col_answer;//輸了也要把正確答案寫出來
                        data->flag = -1;//結束
                    }
                    else{//child process設定正確答案answer parent猜的data->idx
                        if(row_answer > data->row_idx){
                            data->row_hint = 1;//row值比較小
                            sprintf(buffer + strlen(buffer), " down");
                        }
                        else if(row_answer < data->row_idx){
                            data->row_hint = -1;//row值比較大
                            sprintf(buffer + strlen(buffer), " up");
                        }
                        else if(row_answer == data->row_idx){
                            data->row_hint = 0;//row值一樣大
                        }
                        
                        if(col_answer > data->col_idx){//col值比較小
                            data->col_hint = 1;
                            sprintf(buffer + strlen(buffer), " right");
                        }
                        else if(col_answer < data->col_idx){//col值比較大
                            data->col_hint = -1;
                            sprintf(buffer + strlen(buffer), " left");
                        }
                        else
                            data->col_hint = 0;//猜的正確
                        
                        sprintf(buffer + strlen(buffer), "\n");
                        write(1, buffer, strlen(buffer));
                        data->flag = 1;//flag ==1--->parent process 沒猜中繼續猜 flag==-1--->parent次數用完結束不能猜  flag==0--->child process對答案給提示
                    }
                }
            }
        }

        munmap(data, SIZE);//已經fork了---->關閉share memory
        _exit(EXIT_SUCCESS);//child process killed
    }
    else {
        srand(seed + 1);//多random一次,讓值不同
        int status, my_pid = getpid();
        sprintf(buffer,"[%d Parent]: Create a child %d\n", my_pid, pid);
        write(1, buffer, strlen(buffer));
        data->flag = 0;//換到child process

        while(1){
            if(data->flag){//data-flag==0---->child process在做事--->等待parent process猜完---->data-flag==1---->進迴圈
                if(data->flag == -1){//parent process還沒猜對,且遊戲結束
                    sprintf(buffer,"[%d Parent]: Target [%d,%d]\n", my_pid, data->row_idx, data->col_idx);
                    write(1, buffer, strlen(buffer));
                    break;
                }
                else{//flag
                    if(data->row_hint){//data_hint!=0--->猜不對
                        if(data->row_hint > 0)//答案比data_hint大
                            data->row_lower_bound = data->row_idx + 1;
                        else if(data->row_hint < 0)//答案比data_hint小
                            data->row_upper_bound = data->row_idx - 1;
                        data->row_idx = rand() % (data->row_upper_bound + 1 - data->row_lower_bound) + data->row_lower_bound;
                    }
                    
                    if(data->col_hint){
                        if(data->col_hint > 0)//答案比data_hint大
                            data->col_lower_bound = data->col_idx + 1;
                        else if(data->col_hint < 0)//答案
                            data->col_upper_bound = data->col_idx - 1;
                        data->col_idx = rand() % (data->col_upper_bound + 1 - data->col_lower_bound) + data->col_lower_bound;
                    }
                    
                    sprintf(buffer,"[%d Parent]: Guess [%d,%d]\n", my_pid, data->row_idx, data->col_idx);
                    write(1, buffer, strlen(buffer));
                    data->flag = 0;
                }
            }
        }

        (void)waitpid(pid, &status, 0);//等child process結束,parent proceess再結束--->防止 zombie process
        munmap(data, SIZE);//關閉share memory跟processes的連結
        shm_unlink(NAME);//把share memory 清掉
        exit(EXIT_SUCCESS);//其實就是return 0 (因為是int main還是確保有回傳值,若是void(main)就不用這行)
    }
}
#include<iostream>
#include<unistd.h>
#include<sys/timerfd.h>
#include<fcntl.h>
#include<cstdint>

int main(){
    //int timerfd_create(int clockid, int flags);
    int timerfd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK);
    if(timerfd < 0){
        perror("timerfd_create error");
        return -1;
    }
    //int timerfd_settime(int timerfd, int flags, const struct itimerspec *it, struct itimerspec *old);
    struct itimerspec itime;
    // 周期性超时的时间间隔
    itime.it_interval.tv_sec = 1;
    itime.it_interval.tv_nsec = 0;//第一次超时时间为1秒后
    // 第一次超时的时间
    itime.it_value.tv_sec = 1;
    itime.it_value.tv_nsec = 0;//第一次超时后，每次超时的时间间隔为1秒
    int ret = timerfd_settime(timerfd, 0,&itime,NULL);
    if(ret < 0){
        perror("timerfd_settime error");
        return -1;
    }

    while(1){
        uint64_t times;
        ret = read(timerfd, &times, sizeof(times));
        if(ret < 0){
            perror("read error");
            return -1;
        }
        std::cout<<"已超时，距离上次超时"<<times<<"次"<<std::endl;
        sleep(1);
    }

    return 0;
}
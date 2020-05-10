CS 537 Spring 2020, The Extra Credit Project: Semaphores
========================================================

In this project, you will code functions to implement semaphores: first you will initialize a set of semaphores using `sem_init()` function. Then you will code up `sem_wait()` and `sem_post()` functions which either puts the thread to sleep or wakes it up, `sem_destroy()` to destroy semaphores … And that’s it!

Administrivia
-------------

This is an extra credit, optional project for **5%** of the course (see Piazza for more grading details), to be done individually! Also note that there are *NO SLIP DAYS* for this project.

Overview
--------

The prior projects that we did on xv6 all had us running single threaded programs on xv6. Now you are given a code base which has support for real kernel threads in xv6! – Isn’t it fun when that happens without you writing a single line of code?!

### Background

Specifically there are three things that have changed since our last xv6 project. First, there is a new system call to create a kernel thread, called clone(), as well as one to wait for a thread called join(). Then, using clone() there is now a little thread library, with a `thread_create()` and `thread_join()` function. That sets the stage for us to implement some concurrency primitives like semaphores! (P.S: In case you are curious you can even read how clone and join work in kernel/proc.c!)

The goal of this project is to implement semaphores in the xv6 kernel and make them available to user programs. In case you need to refresh your memory on what semaphores are, [the textbook chapter](http://pages.cs.wisc.edu/~remzi/OSTEP/threads-sema.pdf) and our class notes should be a good starting point.

### Semaphore API Details

As the semaphore functions are to be implemented in kernel space, you need to define system calls to achieve the following functions.

1.  `sem_init(int* sem_id, int count)`: There are a limited number of semaphores that will be available in our OS (defined as `NUM_SEMAPHORES` in include/types.h ). When users want to create a semaphore they call `sem_init` and the kernel searches for an unused semaphore. If one is found, its value is initialized to `count`, its ID is filled by the kernel in `sem_id` and the system call returns 0. If no such semaphore can be found, the system call returns -1.
2.  `sem_wait(int sem_id)`: This first decrements the count by 1 for the semaphore given by `sem_id`. If the count becomes negative, the kernel puts the calling thread to sleep. Read about the `sleep()` function as implemented in xv6 `kernel/proc.c`. If `sem_id` is not a valid semaphore this system call again returns -1. **Update: It is also fine to implement the semantics where the calling thread sleeps until the count is positive and then decrements the count. This is also described as Zemaphores in the [textbook](http://pages.cs.wisc.edu/~remzi/OSTEP/threads-sema.pdf) / [class notes](http://pages.cs.wisc.edu/~shivaram/cs537-sp20-notes/semaphore/cs537-semaphore-notes.pdf).**
3.  `sem_post(int sem_id)`: This function increments the count by 1 for the semaphore given by `sem_id` and wakes up a thread (if any) waiting on the semaphore. Read about `wakeup()` function from `kernel/proc.c` to know more about how to wake up threads waiting in the kernel. If `sem_id` is not a valid semaphore this system call again returns -1.
4.  `sem_destroy(int sem_id)`: This function ‘destroys’ or wipes out the semaphore given by `sem_id`. Once a semaphore is destroyed it can be reused in a future call to `sem_init`.

Just to summarize the userspace API for your system calls will look like

    int sem_init(int* sem_id, int count);
    int sem_wait(int sem_id);
    int sem_post(int sem_id);
    int sem_destroy(int sem_id);

And here is a simple test program that shows how we can use semaphores to make the parent thread wait for a child thread as we discussed in class.

    #include "types.h"
    #include "user.h"

    int sem_id;

    void
    func(void *arg1, void *arg2)
    {
      printf(1, "child\n");
      sem_post(sem_id);
      exit();
    }

    int main(int argc, char *argv[]){
        int arg1 = 0xABCDABCD;
        int arg2 = 0xCDEFCDEF;

        int count = 0;
        if(sem_init(&sem_id,count) < 0){
            printf(1, "main: error initializing semaphore\n");
            exit();
        }

        printf(1, "got assigned sem id %d\n", sem_id);

        int pid1 = thread_create(&func, &arg1, &arg2);
        if (pid1 < 0) {
          exit();
        }
        sem_wait(sem_id);
        printf(1, "parent: end\n");
        thread_join();
        exit();
    }

### Hints

The hints are already present in the writeup above! Reading the sleep and wakeup calls carefully and understanding their semantics should help you a lot. Also reading how to create and use spin locks inside the kernel will be helpful. We’ll keep our hints section short given this whole project is for extra credit!

Code
====

For this project, we give you a copy of xv6 which has the `clone()` and `join()` functions implemented. You do not need to change these functions. Copy the xv6 version from `~cs537-1/xv6-sp20-ec` to start your assignment. Details on test cases will also be posted to Piazza.

Due Date
========

April 29, 2020 at 10pm.

Submitting your implementation
==============================

The handin directory is `~cs537-1/handin/<cs-login>/ec/`. To submit the solutions copy all xv6 files and directories with your changes to the handin directory. One way to do this would be to navigate to your solution and execute

    cp -r . ~cs537-1/handin/<cs-login>/ec/
    cd ~cs537-1/handin/<cs-login>/ec/ && make && make clean

Consider the following when submitting the solution -

-   There are NO SLIP DAYS for this project.
-   Your files should be directly copied to `~cs537-1/handin/<cs-login>/ec/` directory. Having subdirectories in `<cs-login>/ec/` like `<cs-login>/ec/xv6-sp20` is **not acceptable**.

Acknowledgement
===============

The project uses material created by Prof. Remzi and Prof. Swift for their OS class offerring in Spring 2018 and Spring 2009 respectively.

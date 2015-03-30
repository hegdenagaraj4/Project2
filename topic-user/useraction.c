/*
 * CS 551 : Project 2: Minix IPC
 * Team: Group 1: Nagaraj, Darshan, Sairam
 * useractions.c: user interactions.
 */

#include "debug.h"
#include <stdlib.h>
#include <sys/time.h>
#include "common.h" /*new file*/
#include <signal.h>

/*
**function      : main()
**params        : none
**returnVal     : int 
**description   : entry point for user program. 
*/
int main()
{
    int choice =0;/*choose from the options provided*/
    struct sigaction sa;
    struct itimerval timer;

    /* Install timer_handler as the signal handler for SIGVTALRM. */
    memset (&sa, 0, sizeof (sa));
    sa.sa_handler = &timer_handler;
    sigaction (SIGALRM, &sa, NULL);
    /* Configure the timer to expire after 15 min --> 900 sec. */
    timer.it_value.tv_sec = 900;
    timer.it_value.tv_usec = 0;
    /* ... and every 250 msec after that. */
    timer.it_interval.tv_sec = 0;
    timer.it_interval.tv_usec = 250000;

    while(1)
    {
        int c;

        log_info("Please enter your choice");
        printf(" 1:Topic Lookup\n 2:Topic Create\n 3:Register as Publisher\n"
               " 4:Register as Subscriber\n 5:Publish\n 6:Read\n 7:Exit \n>");
        scanf("%d",&choice);
        switch(choice)
        {
            case 1: /* Topic Lookup */
   	    {
                setitimer(ITIMER_REAL, &timer, NULL);
                if(topic_Lookup()!=0)
                {
                    log_err("topic_Lookup failed");
                } 
                alarm(0);
            }
            break;

            case 2: /* Topic Create */
            {
                setitimer(ITIMER_REAL, &timer, NULL);
                if(topic_Create()!=0)
                {
                   log_err("Failed to create topic");
                }
                else
                {
                    log_info("Successful\n");
                }
                alarm(0);
            }
            break;

            case 3: /* Register as Publisher */
            {
                setitimer(ITIMER_REAL, &timer, NULL);
                if(registerAs_Publisher()!=0)/*to register as a Publisher*/
                {
                    log_err("registerAs_Publisher failed");
                }
                else
                {
                    log_info("Your registration as a publisher is successful");
                }
                alarm(0);
            }
            break;

            case 4: /* Register as Subscriber */
            {
                setitimer(ITIMER_REAL, &timer, NULL);
                if(registerAs_Subscriber()!=0)/*to register as a Subscriber*/
                {
                    log_err("registerAs_Subscriber failed");
                }
                else
                {
                    log_info("Your registration as a Subscriber is successful\n");
                }
                alarm(0);
            }
            break;

            case 5: /* Publish */
            {
                setitimer(ITIMER_REAL, &timer, NULL);
                if(publish_Topic()!=0)
                {
                    log_err("Something failed ");
                }
                else
                {
                    log_info("Publishing topic successful\n");
                }
                alarm(0);
            }
            break;

            case 6: /* Read/Retrieve */
            {
                setitimer(ITIMER_REAL, &timer, NULL);
                if(read_Topic()!=0)
                {
                    log_err("Could not read");
                }
                else
                {
                    log_info("Successful");
                }
                alarm(0);
            }
            break;

            case 7: /* Exit */
            {
                log_info("***************EXITING************");
                if(topic_clean()!=0)
                {
                     log_err("clean Topic failed");
                }
                exit(0);
            }
            break;

            default:
            {
                log_info("Wrong choice. Please enter the correct choice");
                choice = -1;
            }
            break;
        }
        /* clean stdin buffer */
        while ((c = getchar()) != '\n' && c != EOF);
    }

    return 0;
}

/*
**function      : timer_handler()
**params        : signal number
**returnVal     : none 
**description   : handler when timer kicks in. 
*/
void timer_handler (int signum)
{
    static int count = 0;
    printf ("timer expired we are in deadlock exiting \n", ++count);
    alarm(0);
    exit(0);
}

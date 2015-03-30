/*
 * CS 551 : Project 2: Minix IPC
 * Team: Group 1: Nagaraj, Darshan, Sairam
 * common.h: header file for user functions..
 */

#ifndef _TOPIC_
#define _TOPIC_
#include <lib.h>
int registerAs_Publisher();
int registerAs_Subscriber();
int read_Topic();
int publish_Topic();
void timer_handler (int signum);
#endif

/*
 * CS 551 : Project 2: Minix IPC
 * Team: Group 1: Nagaraj, Darshan, Sairam
 * useractions.c: user helper functions.
 */

#include "debug.h"
#include <stdlib.h>
#include "common.h"

#define MAX_SIZE 16 
#define MAX_GROUPS 4 

int f_pubgroup_id[MAX_GROUPS]={-1}; /* list of groups with which user registered
as publisher */
int f_group_id[MAX_GROUPS]={-1}; /* list of available groups in the system */
int f_subgroup_id[MAX_GROUPS]={-1}; /* list of groups with which user registered
as subscriber */
int f_num_recgroups_available; /* number of subscribed groups */
int f_num_pubgroups_available; /* number of publisher groups */

/*
**function      : topic_Lookup()
**params        : none
**returnVal     : integer
**description   : allows the user to lookup on the topic
*/
int topic_Lookup()
{
    int ret=0;
    message m;
    char *retval;
    int i=0;
    int available_groups;

    log_entry();
    do
    {
        for (i = 0; i < MAX_GROUPS; i++) {
            f_group_id[i]=-1;
        }
        /* lookup for available groups */
        retval = _syscall(PM_PROC_NR,PM_TOPIC_LOOKUP,&m);
        if(retval)
        {
            ret =-1;
            log_err("");
            break;
        }
        available_groups=m.m_tlk.avbl_groups;
        if(available_groups==0)
        {
            ret=0;
            printf("No groups available\n");
            break;
        }
        for(i=0;i<available_groups;i++)
        {
            f_group_id[i] = m.m_tlk.gid[i];
        }

        printf("Available Groups: ");
        for(i=0;(i<MAX_GROUPS)&&(f_group_id[i]!=-1);i++) {
            printf("%d ", f_group_id[i]);
        }
        printf("\n");

    }while(0);

    log_exit("return value is %s",(ret ==0)?"Success":"Failure");
    return ret;
}

/*
**function      : topic_Create()
**params        : none
**returnVal     : integer
**description   : allows the user to create a topic
*/
int topic_Create()
{
    int ret=0;
    message m;
    log_entry();
    do
    {
        char *retVal;
        /* create a topic */
        retVal = _syscall(PM_PROC_NR,PM_TOPIC_CREATE,&m);
        if(retVal)
        {
            ret = -1;
            if (errno == ENOSPC) {
                log_err("MAX_GROUPS reached");
            } else {
                log_err(" ");
            }
            break;
        }

        if(f_num_pubgroups_available < MAX_GROUPS)
        {
            debug("group id is %d",m.m_tcr.gid);
            f_pubgroup_id[f_num_pubgroups_available++] = m.m_tcr.gid;
            printf("Group with ID %d created\n",m.m_tcr.gid);
        }
        else
        {
            debug("MAX_GROUPS reached %d", f_num_pubgroups_available);
        }

    }while(0);

    log_exit("return value is %s",(ret ==0)?"Success":"Failure");
    return ret;
}

/*
**function      : registerAs_Publisher()
**params        : none
**returnVal     : integer
**description   : allows the user to register as a publisher and a group id to
                  which the user belongs is returned.
*/
int registerAs_Publisher()
{
    int ret=0, i = 0;
    message m;

    log_entry();
    do
    {
        int group_choice=0, unpgroups = 0;
        int f_unpgroup_id[MAX_GROUPS]={-1};

        if(topic_Lookup())
        {
            ret =-1;
            log_err("");
            break;
        }

        printf("Already Publisher of Groups: ");
        for(i=0;(i<f_num_pubgroups_available)&&(f_num_pubgroups_available<=MAX_GROUPS);i++)
        {
            printf("%d ", f_pubgroup_id[i]);
        }
        printf("\n");

        printf("Choose a group to register as publisher: ");
        scanf("%d",&group_choice);

        m.m_tpr.gid = group_choice;
        int retval;
        /* Register as publisher */
        retval = _syscall(PM_PROC_NR,PM_TOPIC_PUBLISHER,&m);
        if(retval)
        {
            if (errno == EINVAL || errno == ENOENT) {
                log_err("No such Group exists");
            } else if (errno == EEXIST) {
                log_err("Already a Publisher of group %d", group_choice);
            } else if (errno == ENOSPC) {
                log_err("No more publishers allowed for group %d", group_choice); 
            } else {
                log_err(" ");
            }
            ret =-1;
            break;
        }
        if(f_num_pubgroups_available < MAX_GROUPS)
        {
            debug("group id is %d",group_choice);
            f_pubgroup_id[f_num_pubgroups_available++] = group_choice;
        }
    }while(0);

    log_exit("return value is %s",(ret ==0)?"Success":"Failure");
    return ret;
}

/*
**function      : registerAs_Subscriber()
**params        : none
**returnVal     : integer
**description   : allows the user to register as a subscriber and a group id to
                  which the user belongs is returned.
*/
int registerAs_Subscriber()
{
    int retVal=0,i=0;
    message m;

    log_entry();
    do
    {
        int choice=0;

        if(topic_Lookup())
        {
            retVal =-1;
            log_err("");
            break;
        }

        printf("Already Subscriber of Groups: ");
        for(i=0;(i<f_num_recgroups_available)&&(f_num_recgroups_available<=MAX_GROUPS);i++)
        {
            printf("%d ", f_subgroup_id[i]);
        }
        printf("\n");

        printf("Choose a group to subscribe for: ");
        scanf("%d",&choice);

        m.m_tsr.gid = choice;
        /* Register as subscriber */
        retVal = _syscall(PM_PROC_NR,PM_TOPIC_SUBSCRIBER,&m);
        if(retVal)
        {
            if (errno == EINVAL || errno == ENOENT) {
                log_err("No such Group exists");
            } else if (errno == EEXIST) {
                log_err("Already a Subscriber of group %d", choice);
            } else if (errno == ENOSPC) {
                log_err("No more subscribers allowed for group %d", choice); 
            } else {
                log_err(" ");
            }
            retVal =-1;
            break;
        }
        if(f_num_recgroups_available < MAX_GROUPS)
            f_subgroup_id[f_num_recgroups_available++] = choice;

    }while(0);

    log_exit("return value is %s",(retVal ==0)?"Success":"Failure");
    return retVal;
}

/*
**function      : read_topic()
**params        : none
**returnVal     : integer
**description   : allows the user to read a topic that has been published by the user if it is in the
                  same group.
*/
int read_Topic()
{
    int ret=0,i=0,subgroupid=0;
    message m;

    log_entry();
    do
    {
        printf("Subscribed to groups: ");
        for(i=0;i<f_num_recgroups_available;i++)
        {
            printf("%d ",f_subgroup_id[i]);
        }
        printf("\nplease enter group id: ");
        scanf("%d",&subgroupid);

        {
again:        
            m.m_tr.gid =  subgroupid;
            char *retval;
            /* Read/Retrieve a message */
            retval = _syscall(PM_PROC_NR,PM_TOPIC_RETRIEVE,&m);
            if(retval)
            {
                ret = -1;
                if (errno == EINVAL) {
                    log_err("No such Group exists");
                } else if (errno == EACCES) {
                    log_err("Not a registered Subscriber of group %d",
                            subgroupid);
                } else if (errno == ENOENT) {
                    log_err("No more messages in group %d", subgroupid);
                    ret = 0;
                } else if (errno == EWOULDBLOCK) {
                    log_err("Publisher is working on group %d", subgroupid);
                    goto again;
                } else {
                    log_err(" ");
                }
                break;
            }
            printf("Successfully read the message:\n");
            printf("sequence number is %d\n",m.m_tr.seqno);
            printf("sender pid is %d\n",m.m_tr.senderpid);
            printf("message is %s\n",m.m_tr.topic);
        }
        if(ret)
        {
            log_err("something failed!!");
            break;
        }

    }while(0);

    log_exit("return value is %s",(ret ==0)?"Success":"Failure");
    return ret;
}

/*
**function      : publish_Topic()
**params        : none
**returnVal     : integer
**description   : allows the user to publish a topic if the user belongs to the same
                  group as received on registerAs_Publish.
*/
int publish_Topic()
{
    int ret=0;
    message m;
    int i=0;
    int choice=0;

    log_entry();
    do
    {
        int group_id=0;
        char string[256];
        char *retval;

        printf("Registered as publisher to groups: ");
        for(i=0;i<f_num_pubgroups_available;i++)
        {
            printf("%d",f_pubgroup_id[i]);
        }

        printf("\nPlease enter the group id you would like to publish to: ");
        scanf("%d",&group_id);

        printf("Please enter the message: ");
        retval = scanf("%s",&string);
        string[MAX_SIZE-1] = '\0';

    	log_info("input string contains %s.. %d",string, group_id);
        if(retval)
        {
again:
            m.m_tp.gid = group_id;
            strcpy(m.m_tp.topic,string);
            /* publish a message */
            retval = _syscall(PM_PROC_NR,PM_TOPIC_PUBLISH,&m);
            if(retval)
            {
                ret = -1;
                if (errno == EINVAL) {
                    log_err("No such Group exists");
                } else if (errno == EACCES) {
                    log_err("Not a registered Publisher of group %d",
                            group_id);
                } else if (errno == ENOSPC) {
                    log_err("No buffer space in group %d", group_id);
                } else if (errno == ENOMEM) {
                    log_err("Memory not available to write. group %d", group_id);
                } else if (errno == EWOULDBLOCK) {
                    log_err("Publisher is working on group %d", group_id);
                    goto again;
                } else {
                    log_err(" ");
                }
                break;
            }
        }
        else
        {
            log_err("Could not get user input ");
            ret =-1;
            break;
        }
    }while(0);
    log_exit("return value is %s",(ret ==0)?"Success":"Failure");
    return ret;
}

/*
**function      : topic_clean()
**params        : none
**returnVal     : integer
**description   : cleans/resets kernel state.
*/
int topic_clean()
{
    int ret=0;
    message m;
    log_entry();
    do
    {
        char *retval;
        /* clean kernel state */
        retval = _syscall(PM_PROC_NR,PM_TOPIC_CLEAN,&m);
        if(retval)
        {
            log_err(" ");
            ret = -1;
            break;
        }

    }while(0);
    log_exit("return value is %s",(ret ==0)?"Success":"Failure");
    return ret;
}

/*
 * CS 551 : Project 2: Minix IPC
 * Team: Group 1: Nagaraj, Darshan, Sairam
 * topic.c: topic related syscalls implementation.
 */

#include "pm.h"
#include "mproc.h"
#include <minix/mthread.h>

#define EOK 0
#define mutex_t		mthread_mutex_t
#define mutex_init	mthread_mutex_init
#define mutex_destroy	mthread_mutex_destroy
#define mutex_lock	mthread_mutex_lock
#define mutex_trylock	mthread_mutex_trylock
#define mutex_unlock	mthread_mutex_unlock
#define debug 1
#define log(l, S, ...) { \
    if ((l == 1) || debug) { \
        printf("[DEBUG: %s, %d] [pid %d]: " S "\n", __func__,\
                __LINE__, mp ? mp->mp_pid : -1, ##__VA_ARGS__);\
    }\
}

/*
 * groups:
 * Each group will contain MAX_TOPICS_PER_GROUP topics. Users can register as
 * publishers/subscribers for a group.
 * topic:
 * topic holds the message.
 */
#define MAX_TOPICS_PER_GROUP 5 /* Max topics/messages per group */
#define MAX_TOPIC_SIZE 16      /* Max topic/message size */ 
#define MAX_PROCESSES 4        /* Max users per group */
#define MAX_GROUPS 4           /* Max groups */ 
#define GROUPID_BASE 0x1000    /* Group ID base */ 

typedef struct topic_ {
    struct topic_ *nt; /* points to next topic in list */
    int seqno; /* sequence number of the topic */
    int senderpid;  /* pid of the sender */
    char topic[MAX_TOPIC_SIZE]; /* topic message */
    pid_t pending_rpids[MAX_PROCESSES]; /* pending receiver pids */
    int pending_receivers; /* number of receivers yet to see message */
} topic_t;

typedef struct group_ {
    mutex_t lock; /* group specific lock */
    int ID; /* ID of the topic/group */
    int num_publishers; /* total # of senders */
    int num_subscribers; /* total # of receivers */
    int num_topics; /* number of topics in group */
    pid_t sregisters_pid[MAX_PROCESSES]; /* senders pid */
    pid_t rregisters_pid[MAX_PROCESSES]; /* receivers pid */
    int cur_seqno;  /* running sequence number of topic */
    int active_publisher; /* 1 - when publisher is publishing a message. 0 -
                           * otherwise*/
    int active_subscribers; /* # of sctive subscribers */
    topic_t *tf; /* points to first topic in list */
    topic_t *tl; /* points to last topic in list */
} group_t;

int num_groups; /* Total number of active groups */
group_t Grp[MAX_GROUPS]; /* Global array of groups */
mutex_t global_lock; /* global loack to */
int global_lock_init = 0; /* indicates whether global lock is initialized or not */

/*
 * do_topiclookup:
 * populates m_tlk.gid with available group ID's and sends to user
 */
int
do_topiclookup(void)
{
    int data_idx = 0;
    int grp_idx = 0;
    register struct mproc *rmp = mp;

    for (grp_idx = 0; grp_idx < num_groups; grp_idx++) {
        if (Grp[grp_idx].ID & GROUPID_BASE) {
            rmp->mp_reply.m_tlk.gid[data_idx] = Grp[grp_idx].ID;
            data_idx++;
            log(0, "idx %d, ID %d", data_idx - 1, Grp[grp_idx].ID);
            if (data_idx >= 13) { //m_tlk.gid[13]
                break;
            }
        }
    }
    rmp->mp_reply.m_tlk.avbl_groups = data_idx;
    log(0, "Total available groups.. %d", data_idx);
    return EOK;
}

/*
 * do_topicclean:
 * clears/resets kernel state related to groups and topics
 */
int
do_topicclean(void)
{
    int g;

    if (num_groups == 0) {
        /* nothing to clean */
        return EOK;
    }

    /* pick global lock before manipulating num_groups */
    while (mutex_trylock(&global_lock) != 0);
        
    for(g = num_groups - 1; g >= 0; g--) {
        log(0, "cleanup.. group %d", g + 1);
        /* pick group specific lock */
        while (mutex_trylock(&(Grp[g].lock)) != 0);
       
        /* free topics/messages */
        while(Grp[g].tl != NULL) {
            topic_t *t = Grp[g].tl;
            Grp[g].tl = t->nt;
            free(t);
        }

        /* unlock and destroy group specific lock */
        mutex_unlock(&(Grp[g].lock));
        mutex_destroy(&(Grp[g].lock));
        num_groups--;
        memset(&Grp[g], 0, sizeof(group_t)); /* reset */
    }
    log(0, "cleanup.. num_groups %d", num_groups);
    /* All done.. drop global lock */
    mutex_unlock(&global_lock);
    return EOK;
}

/*
 * do_topiccreate:
 * creates a group.
 */
int
do_topiccreate(void)
{
    int num_publishers;
    register struct mproc *rmp = mp;

    if (global_lock_init == 0) {
        /* global lock is not initialized. initilaize it. */
        if (mutex_init(&global_lock, NULL) != 0) {
            log(1, "Failed to init Global lock");
            return ENOLCK;
        }
        /* remember that global lock is initialized */
        global_lock_init++;
    }

    /* pick global lock before manipulating num_groups */
    while (mutex_trylock(&global_lock) != 0);

    if (num_groups + 1 > MAX_GROUPS) {
        /* drop global lock before returning */
        mutex_unlock(&global_lock);
        log(1, "Total groups are already max.. %d", num_groups);
        return ENOSPC;
    }

    memset(&Grp[num_groups], 0, sizeof(group_t));

    /* initialize group specific lock */
    if (mutex_init(&(Grp[num_groups].lock), NULL) != 0) {
        log(1, "Failed to init group %d lock", num_groups);
        return ENOLCK;
    }

    num_publishers = Grp[num_groups].num_publishers;

    Grp[num_groups].ID = GROUPID_BASE + num_groups;
    Grp[num_groups].sregisters_pid[num_publishers] = rmp->mp_pid;
    Grp[num_groups].num_publishers++;

    rmp->mp_reply.m_tcr.gid = Grp[num_groups].ID;
    log(0, "Created group(%d). ID %d, publishers %d", num_groups,
        Grp[num_groups].ID, Grp[num_groups].num_publishers);
    num_groups++;
    /* All done.. drop global lock */
    mutex_unlock(&global_lock);
    return EOK;
}

/*
 * do_topicpublisher:
 * register a user as publisher.
 */
int
do_topicpublisher(void)
{
    int num_publishers, p, ret;
    int interested_grp = m_in.m_tpr.gid - GROUPID_BASE;
    register struct mproc *rmp = mp;

    /* check whether valid interested group or not */
    if ((num_groups == 0) || (0 > interested_grp) || (interested_grp >= num_groups)) {
        log(1, "Not a valid interest group (%d). ID %d", interested_grp,
            m_in.m_tpr.gid);
        return EINVAL;
    }

    /* pick up group specific lock */
    while (mutex_trylock(&(Grp[interested_grp].lock)) != 0);

    num_publishers = Grp[interested_grp].num_publishers;

    /* num_publishers = 0 implies this group is yet to be created. */
    if (num_publishers == 0) {
        ret = ENOENT;
        log(1, "No publishers. So, no such group (%d) created.. ID %d",
            interested_grp, m_in.m_tpr.gid);
        goto error;
    }

    /* check if this user is already registered as publisher */
    for (p = 0; p < num_publishers; p++) {
        if (Grp[interested_grp].sregisters_pid[p] == rmp->mp_pid) {
            ret = EEXIST;
            log(1, "Already registered as publisher for this group (%d).. ID %d",
                interested_grp, m_in.m_tpr.gid);
            goto error;
        }
    }
    
    /* check whethere we can allow one more publisher for this group or not */
    if (num_publishers + 1 > MAX_PROCESSES) {
        ret = ENOSPC;
        log(1, "Already have max(%d) publishers for this group (%d).. ID %d",
            num_publishers, interested_grp, m_in.m_tpr.gid);
        goto error;
    }

    Grp[interested_grp].sregisters_pid[num_publishers] = rmp->mp_pid;
    Grp[interested_grp].num_publishers++;
    ret = EOK;
    log(0, "Registered as a publisher for group (%d).. ID %d. "
        "Total publishers %d", interested_grp, m_in.m_tpr.gid,
        Grp[interested_grp].num_publishers);

error:
    /* drop group specific lock before returning. */
    mutex_unlock(&(Grp[interested_grp].lock));
    return ret;
}

/*
 * do_topicsubscriber:
 * register as subscriber for a group.
 */
int
do_topicsubscriber(void)
{
    int num_subscribers, s, ret;
    int interested_grp = m_in.m_tsr.gid - GROUPID_BASE;
    register struct mproc *rmp = mp;

    /* check whether valid interested group or not */
    if ((num_groups == 0) || (0 > interested_grp) || (interested_grp >= num_groups)) {
        log(1, "Not a valid interest group (%d). ID %d", interested_grp,
            m_in.m_tsr.gid);
        return EINVAL;
    }

    /* pick group specific lock */
    while (mutex_trylock(&(Grp[interested_grp].lock)) != 0);

    /* check whether there are senders or not. If no that means group is not created */
    if (Grp[interested_grp].num_publishers == 0) {
        ret = ENOENT;
        log(1, "No publishers. So, no such group (%d) created.. ID %d",
            interested_grp, m_in.m_tsr.gid);
        goto error;
    }

    num_subscribers = Grp[interested_grp].num_subscribers;

    /* check if alresy registered as subscriber or not. If yes, then bail out */
    for (s = 0; s < num_subscribers; s++) {
        if (Grp[interested_grp].rregisters_pid[s] == rmp->mp_pid) {
            ret = EEXIST;
            log(1, "Already registered as subscriber for this group (%d).. ID %d",
                interested_grp, m_in.m_tsr.gid);
            goto error;
        }
    }

    /* check if we can allow one more subscriber into this group or not. */
    if (num_subscribers + 1 > MAX_PROCESSES) {
        ret = ENOSPC;
        log(1, "Already have max(%d) subscribers for this group (%d).. ID %d",
            num_subscribers, interested_grp, m_in.m_tsr.gid);
        goto error;
    }

    Grp[interested_grp].rregisters_pid[num_subscribers] = rmp->mp_pid;
    Grp[interested_grp].num_subscribers++;
    ret = EOK;
    log(0, "Registered as a subscriber for group (%d).. ID %d. "
        "Total subscribers %d", interested_grp, m_in.m_tsr.gid,
        Grp[interested_grp].num_subscribers);

error:
    /* drop group specific lock before returning. */
    mutex_unlock(&(Grp[interested_grp].lock));
    return ret;
}

/*
 * do_topicpublish:
 * publish a message/topic to the group.
 */
int
do_topicpublish(void)
{
    int num_publishers, p, s, interested_grp, pending_r, tidx;
    register struct mproc *rmp = mp;
    void *src, *dst;
    topic_t *t;

    interested_grp = m_in.m_tp.gid - GROUPID_BASE;
    /* check whether valid interested group or not */
    if ((num_groups == 0) || (0 > interested_grp) || (interested_grp >= num_groups)) {
        log(1, "Not a valid interest group (%d). ID %d", interested_grp,
            m_in.m_tp.gid);
        return EINVAL;
    }

    /* check whether this user can publish or not */
    num_publishers = Grp[interested_grp].num_publishers;
    for (p = 0; p < num_publishers; p++) {
        if (Grp[interested_grp].sregisters_pid[p] == rmp->mp_pid) {
            break;
        }
    }

    if (p == num_publishers) { /* not a valid publisher */
        log(1, "Not a registered publisher of group (%d).. ID %d",
            interested_grp, m_in.m_tp.gid);
        return EACCES;
    }

    if (Grp[interested_grp].num_topics == MAX_TOPICS_PER_GROUP) { /* No space
                                                                   * available to write topic */
        log(1, "Max number of topics(%d) in group (%d).. ID %d",
            MAX_TOPICS_PER_GROUP, interested_grp, m_in.m_tp.gid);
        return ENOSPC;
    }

    if (Grp[interested_grp].active_publisher ||
        Grp[interested_grp].active_subscribers) { /* some other
                                                   * publisher/subscribers  is
                                                   * already working */
        log(1, "Someone is already working on group (%d).. ID %d. Pub %d, Sub %d",
            interested_grp, m_in.m_tp.gid, Grp[interested_grp].active_publisher,
            Grp[interested_grp].active_subscribers);
        return EWOULDBLOCK;
    }

    t = (topic_t *)malloc(sizeof(topic_t));
    if (t == NULL) {
        log(1, "Failed to allocate memory for new topic.. group (%d).. ID %d.",
            interested_grp, m_in.m_tp.gid);
        return ENOMEM;
    }
    memset(t, 0, sizeof(topic_t));

    while (mutex_trylock(&(Grp[interested_grp].lock)) != 0);

    if (Grp[interested_grp].active_publisher ||
        Grp[interested_grp].active_subscribers) { /* some other
                                                   * publisher/subscribers  is
                                                   * already working */
        log(1, "Just now someone started working on group (%d).. ID %d. Pub %d, Sub %d",
            interested_grp, m_in.m_tp.gid, Grp[interested_grp].active_publisher,
            Grp[interested_grp].active_subscribers);
        mutex_unlock(&(Grp[interested_grp].lock));
        free(t);
        return EWOULDBLOCK;
    }
    Grp[interested_grp].active_publisher = 1; /* This publisher is active */
    mutex_unlock(&(Grp[interested_grp].lock));

    pending_r = 0;
    for (s = 0; s < Grp[interested_grp].num_subscribers; s++) {
        if (rmp->mp_pid !=
            Grp[interested_grp].rregisters_pid[s]) {
            t->pending_rpids[pending_r] = Grp[interested_grp].rregisters_pid[s];
            pending_r++;
            log(0, "[%d]: receiver %d", pending_r, t->pending_rpids[pending_r]);
        }
    }

    /* no subscribers. drop the message. */
    if (pending_r == 0) {
        log(0, "No interested subscribers.. Drop message. group (%d).. ID %d",
            interested_grp, m_in.m_tp.gid);
        free(t);
        goto done;
    }

    /* Update Group specific attributes */
    Grp[interested_grp].cur_seqno++;
    Grp[interested_grp].num_topics++;

    /* Update current topic specific attributes */
    t->nt = NULL;
    t->seqno = Grp[interested_grp].cur_seqno;
    t->senderpid = rmp->mp_pid;
    src = m_in.m_tp.topic;
    dst = t->topic;
    memcpy(dst, src, MAX_TOPIC_SIZE);

    t->pending_receivers = pending_r;

    if (Grp[interested_grp].tf == NULL) {
        Grp[interested_grp].tf = t;
        Grp[interested_grp].tl = t;
    } else {
        (Grp[interested_grp].tl)->nt = t;
        Grp[interested_grp].tl = t;
    }
    log(0, "Added topic %p to group (%d).. ID %d."
        " Topic [seq %d, data %s, receivers %d]", t, interested_grp,
        m_in.m_tp.gid, t->seqno, t->topic, pending_r);
    log(0, "Grp. tf %p, tl %p, topics %d", Grp[interested_grp].tf,
        Grp[interested_grp].tl, Grp[interested_grp].num_topics);

    rmp->mp_reply.m_tp.seqno = t->seqno;

done:
    /* All is done. reset that there is no active publisher */
    while (mutex_trylock(&(Grp[interested_grp].lock)) != 0);
    Grp[interested_grp].active_publisher = 0;
    mutex_unlock(&(Grp[interested_grp].lock));
    return EOK;
}

/*
 * do_topicretrieve:
 * retrieve/read a message from group.
 */
int
do_topicretrieve(void)
{
    int interested_grp, num_subscribers, s;
    register struct mproc *rmp = mp;
    void *src, *dst;
    topic_t *t;

    interested_grp = m_in.m_tr.gid - GROUPID_BASE;
    /* check whether valid interested group or not */
    if ((num_groups == 0) || (0 > interested_grp) || (interested_grp >= num_groups)) {
        log(1, "Not a valid interest group (%d). ID %d", interested_grp,
            m_in.m_tr.gid);
        return EINVAL;
    }

    /* check whether this user can retrieve or not */
    num_subscribers = Grp[interested_grp].num_subscribers;
    for (s = 0; s < num_subscribers; s++) {
        if (Grp[interested_grp].rregisters_pid[s] == rmp->mp_pid) {
            break;
        }
    }

    if (s == num_subscribers) { /* not a valid subscriber */
        log(1, "Not a registered subscriber of group (%d).. ID %d",
            interested_grp, m_in.m_tr.gid);
        return EACCES;
    }

    /* if there are no topics then bail out */
    if (Grp[interested_grp].num_topics == 0) {
        log(1, "No topics in group (%d).. ID %d", interested_grp,
            m_in.m_tr.gid);
        return ENOENT;
    }

    if (Grp[interested_grp].active_publisher) { /* an active publisher is
                                                 * present */
        log(1, "An active publisher is working on group (%d).. ID %d", interested_grp,
            m_in.m_tr.gid);
        return EWOULDBLOCK;
    }

    while (mutex_trylock(&(Grp[interested_grp].lock)) != 0);
    if (Grp[interested_grp].active_publisher) { /* an active publisher is
                                                 * present */
        mutex_unlock(&(Grp[interested_grp].lock));
        log(1, "Just now nn active publisher started working on group (%d).. ID %d", interested_grp,
            m_in.m_tr.gid);
        return EWOULDBLOCK;
    }
    Grp[interested_grp].active_subscribers++;
    mutex_unlock(&(Grp[interested_grp].lock));

    t = Grp[interested_grp].tf;
    while(t) {
        int dummy = 0;
        for (s = 0; s < num_subscribers; s++) {
            if (t->pending_rpids[s] == rmp->mp_pid) {
                dummy=1;
                break;
            }
        }
        if(dummy==1) {
            break;
        }
        t = t->nt;
    }

    /* No pending messages to the user. */
    if (t == NULL) {
        while (mutex_trylock(&(Grp[interested_grp].lock)) != 0);
        Grp[interested_grp].active_subscribers--;
        log(1, "No topic to read in group (%d).. ID %d", interested_grp,
            m_in.m_tr.gid);
        mutex_unlock(&(Grp[interested_grp].lock));
        return ENOENT;
    } else {
        rmp->mp_reply.m_tr.seqno = t->seqno;
        rmp->mp_reply.m_tr.senderpid = t->senderpid;
        src = t->topic;
        dst = rmp->mp_reply.m_tr.topic;
        memcpy(dst, src, MAX_TOPIC_SIZE);

        while (mutex_trylock(&(Grp[interested_grp].lock)) != 0);
       
        t->pending_rpids[s] = 0;
        t->pending_receivers--;

        log(0, "Sending topic %p in group (%d).. ID %d "
            " Topic [seq %d, data %s, remaining receivers %d, sender %d]", t, interested_grp,
            m_in.m_tr.gid, t->seqno, t->topic, t->pending_receivers,
            t->senderpid);
        log(0, "Grp. tf %p, tl %p, topics %d. active sub %d", Grp[interested_grp].tf,
            Grp[interested_grp].tl, Grp[interested_grp].num_topics,
            Grp[interested_grp].active_subscribers - 1);
        
        /* free the message if all interested users have seen it. */
        if(t->pending_receivers == 0) {
            if (t == Grp[interested_grp].tf) {
                Grp[interested_grp].tf = t->nt;
                Grp[interested_grp].num_topics--;
                if (Grp[interested_grp].num_topics == 0) {
                    Grp[interested_grp].tf = NULL;
                    Grp[interested_grp].tl = NULL;
                    log(0, "t->nt %p, n_topics %d", t->nt, 0);
                }
                free(t);
                t = NULL;
            }
        }

        log(0, "Grp. tf %p, tl %p, topics %d. active sub %d", Grp[interested_grp].tf,
            Grp[interested_grp].tl, Grp[interested_grp].num_topics,
            Grp[interested_grp].active_subscribers - 1);
        
        Grp[interested_grp].active_subscribers--;
        mutex_unlock(&(Grp[interested_grp].lock));
    }

    return EOK;
}

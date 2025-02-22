/*
 * Copyright (c) 2004-2005 The Trustees of Indiana University and Indiana
 *                         University Research and Technology
 *                         Corporation.  All rights reserved.
 * Copyright (c) 2004-2011 The University of Tennessee and The University
 *                         of Tennessee Research Foundation.  All rights
 *                         reserved.
 * Copyright (c) 2004-2005 High Performance Computing Center Stuttgart,
 *                         University of Stuttgart.  All rights reserved.
 * Copyright (c) 2004-2005 The Regents of the University of California.
 *                         All rights reserved.
 * Copyright (c) 2008      Cisco Systems, Inc.  All rights reserved.
 * Copyright (c) 2012-2013 Los Alamos National Security, LLC.
 *                         All rights reserved.
 * Copyright (c) 2015-2020 Intel, Inc.  All rights reserved.
 * Copyright (c) 2017      IBM Corporation.  All rights reserved.
 * Copyright (c) 2017      Mellanox Technologies. All rights reserved.
 * Copyright (c) 2018      Research Organization for Information Science
 *                         and Technology (RIST).  All rights reserved.
 * Copyright (c) 2021-2022 Nanook Consulting.  All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */
/**
 * @file
 *
 * I/O Forwarding Service
 */

#ifndef PMIX_IOF_H
#define PMIX_IOF_H

#include "src/include/pmix_config.h"

#ifdef HAVE_SYS_TYPES_H
#    include <sys/types.h>
#endif
#ifdef HAVE_SYS_UIO_H
#    include <sys/uio.h>
#endif
#ifdef HAVE_NET_UIO_H
#    include <net/uio.h>
#endif
#ifdef HAVE_UNISTD_H
#    include <unistd.h>
#endif
#include <signal.h>

#include "src/class/pmix_list.h"
#include "src/include/pmix_globals.h"
#include "src/util/pmix_fd.h"

BEGIN_C_DECLS

/*
 * Maximum size of single msg
 */
#define PMIX_IOF_BASE_MSG_MAX        8192
#define PMIX_IOF_BASE_TAG_MAX        1024
#define PMIX_IOF_MAX_INPUT_BUFFERS   50
#define PMIX_IOF_MAX_RETRIES         4

typedef struct {
    pmix_list_item_t super;
    bool pending;
    bool always_writable;
    int numtries;
    pmix_event_t *ev;
    struct timeval tv;
    int fd;
    pmix_list_t outputs;
} pmix_iof_write_event_t;
PMIX_EXPORT PMIX_CLASS_DECLARATION(pmix_iof_write_event_t);
#define PMIX_IOF_WRITE_EVENT_STATIC_INIT    \
{                                           \
    .super = PMIX_LIST_ITEM_STATIC_INIT,    \
    .pending = false,                       \
    .always_writable = false,               \
    .numtries = 0,                          \
    .ev = NULL,                             \
    .tv = {0, 0},                           \
    .fd = 0,                                \
    .outputs = PMIX_LIST_STATIC_INIT        \
}

typedef struct {
    pmix_list_item_t super;
    pmix_proc_t name;
    pmix_iof_channel_t tag;
    pmix_iof_write_event_t wev;
    bool xoff;
    bool exclusive;
    bool closed;
} pmix_iof_sink_t;
PMIX_EXPORT PMIX_CLASS_DECLARATION(pmix_iof_sink_t);
#define PMIX_IOF_SINK_STATIC_INIT               \
{                                               \
    .super = PMIX_LIST_ITEM_STATIC_INIT,        \
    .name = {{0}, 0},                           \
    .tag = PMIX_FWD_NO_CHANNELS,                \
    .wev = PMIX_IOF_WRITE_EVENT_STATIC_INIT,    \
    .xoff = false,                              \
    .exclusive = false,                         \
    .closed = false                             \
}

typedef struct {
    pmix_list_item_t super;
    char *data;
    int numbytes;
} pmix_iof_write_output_t;
PMIX_EXPORT PMIX_CLASS_DECLARATION(pmix_iof_write_output_t);

typedef struct {
    pmix_object_t super;
    pmix_event_t ev;
    struct timeval tv;
    int fd;
    bool active;
    void *childproc;
    bool always_readable;
    pmix_proc_t name;
    pmix_iof_channel_t channel;
    pmix_proc_t *targets;
    size_t ntargets;
    pmix_info_t *directives;
    size_t ndirs;
} pmix_iof_read_event_t;
PMIX_EXPORT PMIX_CLASS_DECLARATION(pmix_iof_read_event_t);

typedef struct {
    pmix_list_item_t super;
    pmix_proc_t name;
    pmix_iof_write_event_t *channel;
    pmix_iof_flags_t flags;
    pmix_iof_channel_t stream;
    bool copystdout;
    bool copystderr;
    pmix_byte_object_t bo;
} pmix_iof_residual_t;
PMIX_EXPORT PMIX_CLASS_DECLARATION(pmix_iof_residual_t);

/* Write event macro's */

static inline bool pmix_iof_fd_always_ready(int fd)
{
    return pmix_fd_is_regular(fd) || (pmix_fd_is_chardev(fd) && !isatty(fd))
           || pmix_fd_is_blkdev(fd);
}

#define PMIX_IOF_SINK_BLOCKSIZE (1024)

#define PMIX_IOF_SINK_ACTIVATE(w)                                      \
    do {                                                               \
        struct timeval *tv = NULL;                                     \
        (w)->pending = true;                                           \
        PMIX_POST_OBJECT((w));                                         \
        if ((w)->always_writable) {                                    \
            /* Regular is always write ready. Use timer to activate */ \
            tv = &(w)->tv;                                             \
        }                                                              \
        if (pmix_event_add((w)->ev, tv)) {                             \
            PMIX_ERROR_LOG(PMIX_ERR_BAD_PARAM);                        \
        }                                                              \
    } while (0);

/* define an output "sink", adding it to the provided
 * endpoint list for this proc */
#define PMIX_IOF_SINK_DEFINE(snk, nm, fid, tg, wrthndlr)                                           \
    do {                                                                                           \
        PMIX_OUTPUT_VERBOSE((1, pmix_client_globals.iof_output,                                    \
                             "defining endpt: file %s line %d fd %d", __FILE__, __LINE__, (fid))); \
        PMIX_CONSTRUCT((snk), pmix_iof_sink_t);                                                    \
        pmix_strncpy((snk)->name.nspace, (nm)->nspace, PMIX_MAX_NSLEN);                            \
        (snk)->name.rank = (nm)->rank;                                                             \
        (snk)->tag = (tg);                                                                         \
        if (0 <= (fid)) {                                                                          \
            (snk)->wev.fd = (fid);                                                                 \
            (snk)->wev.always_writable = pmix_iof_fd_always_ready(fid);                            \
            if ((snk)->wev.always_writable) {                                                      \
                pmix_event_evtimer_set(pmix_globals.evbase, (snk)->wev.ev, wrthndlr, (snk));       \
            } else {                                                                               \
                pmix_event_set(pmix_globals.evbase, (snk)->wev.ev, (snk)->wev.fd, PMIX_EV_WRITE,   \
                               wrthndlr, (snk));                                                   \
            }                                                                                      \
        }                                                                                          \
        PMIX_POST_OBJECT(snk);                                                                     \
    } while (0);

/* Read event macro's */
#define PMIX_IOF_READ_ADDEV(rev)                \
    do {                                        \
        struct timeval *tv = NULL;              \
        if ((rev)->always_readable) {           \
            tv = &(rev)->tv;                    \
        }                                       \
        if (pmix_event_add(&(rev)->ev, tv)) {   \
            PMIX_ERROR_LOG(PMIX_ERR_BAD_PARAM); \
        }                                       \
    } while (0);

#define PMIX_IOF_READ_ACTIVATE(rev) \
    do {                            \
        (rev)->active = true;       \
        PMIX_POST_OBJECT(rev);      \
        PMIX_IOF_READ_ADDEV(rev);   \
    } while (0);

#define PMIX_IOF_READ_EVENT(rv, p, np, d, nd, fid, cbfunc, actv)                                 \
    do {                                                                                         \
        size_t _ii;                                                                              \
        pmix_iof_read_event_t *rev;                                                              \
        PMIX_OUTPUT_VERBOSE((1, pmix_client_globals.iof_output, "defining read event at: %s %d", \
                             __FILE__, __LINE__));                                               \
        rev = PMIX_NEW(pmix_iof_read_event_t);                                                   \
        if (NULL != (p)) {                                                                       \
            (rev)->ntargets = (np);                                                              \
            PMIX_PROC_CREATE((rev)->targets, (rev)->ntargets);                                   \
            memcpy((rev)->targets, (p), (np) * sizeof(pmix_proc_t));                             \
        }                                                                                        \
        if (NULL != (d) && 0 < (nd)) {                                                           \
            PMIX_INFO_CREATE((rev)->directives, (nd));                                           \
            (rev)->ndirs = (nd);                                                                 \
            for (_ii = 0; _ii < (size_t) nd; _ii++) {                                            \
                PMIX_INFO_XFER(&((rev)->directives[_ii]), &((d)[_ii]));                          \
            }                                                                                    \
        }                                                                                        \
        rev->fd = (fid);                                                                         \
        rev->always_readable = pmix_iof_fd_always_ready(fid);                                    \
        *(rv) = rev;                                                                             \
        if (rev->always_readable) {                                                              \
            pmix_event_evtimer_set(pmix_globals.evbase, &rev->ev, (cbfunc), rev);                \
        } else {                                                                                 \
            pmix_event_set(pmix_globals.evbase, &rev->ev, (fid), PMIX_EV_READ, (cbfunc), rev);   \
        }                                                                                        \
        if ((actv)) {                                                                            \
            PMIX_IOF_READ_ACTIVATE(rev)                                                          \
        }                                                                                        \
    } while (0);

#define PMIX_IOF_READ_EVENT_LOCAL(rv, fid, cbfunc, actv)                                         \
    do {                                                                                         \
        pmix_iof_read_event_t *rev;                                                              \
        PMIX_OUTPUT_VERBOSE((1, pmix_client_globals.iof_output, "defining read event at: %s %d", \
                             __FILE__, __LINE__));                                               \
        rev = PMIX_NEW(pmix_iof_read_event_t);                                                   \
        rev->fd = (fid);                                                                         \
        rev->always_readable = pmix_iof_fd_always_ready(fid);                                    \
        *(rv) = rev;                                                                             \
        if (rev->always_readable) {                                                              \
            pmix_event_evtimer_set(pmix_globals.evbase, &rev->ev, (cbfunc), rev);                \
        } else {                                                                                 \
            pmix_event_set(pmix_globals.evbase, &rev->ev, (fid), PMIX_EV_READ, (cbfunc), rev);   \
        }                                                                                        \
        if ((actv)) {                                                                            \
            PMIX_IOF_READ_ACTIVATE(rev)                                                          \
        }                                                                                        \
    } while (0);


PMIX_EXPORT pmix_status_t pmix_iof_flush(void);

PMIX_EXPORT pmix_status_t pmix_iof_write_output(const pmix_proc_t *name, pmix_iof_channel_t stream,
                                                const pmix_byte_object_t *bo);
PMIX_EXPORT void pmix_iof_static_dump_output(pmix_iof_sink_t *sink);
PMIX_EXPORT void pmix_iof_write_handler(int fd, short event, void *cbdata);
PMIX_EXPORT bool pmix_iof_stdin_check(int fd);
PMIX_EXPORT void pmix_iof_read_local_handler(int unusedfd, short event, void *cbdata);
PMIX_EXPORT void pmix_iof_stdin_cb(int fd, short event, void *cbdata);
PMIX_EXPORT pmix_status_t pmix_iof_process_iof(pmix_iof_channel_t channels,
                                               const pmix_proc_t *source,
                                               const pmix_byte_object_t *bo,
                                               const pmix_info_t *info, size_t ninfo,
                                               const pmix_iof_req_t *req);
PMIX_EXPORT void pmix_iof_check_flags(pmix_info_t *info, pmix_iof_flags_t *flags);
PMIX_EXPORT void pmix_iof_flush_residuals(void);

END_C_DECLS

#endif /* PMIX_IOF_H */

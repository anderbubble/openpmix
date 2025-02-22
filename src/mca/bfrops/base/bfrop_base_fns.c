/*
 * Copyright (c) 2004-2005 The Trustees of Indiana University and Indiana
 *                         University Research and Technology
 *                         Corporation.  All rights reserved.
 * Copyright (c) 2004-2006 The University of Tennessee and The University
 *                         of Tennessee Research Foundation.  All rights
 *                         reserved.
 * Copyright (c) 2004-2005 High Performance Computing Center Stuttgart,
 *                         University of Stuttgart.  All rights reserved.
 * Copyright (c) 2004-2005 The Regents of the University of California.
 *                         All rights reserved.
 * Copyright (c) 2015-2020 Intel, Inc.  All rights reserved.
 * Copyright (c) 2019      Mellanox Technologies, Inc.
 *                         All rights reserved.
 * Copyright (c) 2021-2022 Nanook Consulting  All rights reserved.
 * Copyright (c) 2022      IBM Corporation.  All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#include "src/include/pmix_config.h"

#include <stdio.h>
#ifdef HAVE_UNISTD_H
#    include <unistd.h>
#endif

#include "pmix.h"

#include "src/include/pmix_globals.h"
#include "src/mca/preg/preg.h"
#include "src/util/pmix_argv.h"
#include "src/util/pmix_error.h"
#include "src/hwloc/pmix_hwloc.h"

#include "src/mca/bfrops/base/base.h"

/* define two public functions */
PMIX_EXPORT pmix_status_t PMIx_Value_load(pmix_value_t *v,
                                          const void *data,
                                          pmix_data_type_t type)
{
    pmix_bfrops_base_value_load(v, data, type);
    return PMIX_SUCCESS;
}

PMIX_EXPORT pmix_status_t PMIx_Value_unload(pmix_value_t *kv,
                                            void **data,
                                            size_t *sz)
{
    return pmix_bfrops_base_value_unload(kv, data, sz);
}

PMIX_EXPORT void PMIx_Value_destruct(pmix_value_t *val)
{
    pmix_bfrops_base_value_destruct(val);
}

PMIX_EXPORT pmix_status_t PMIx_Value_xfer(pmix_value_t *dest,
                                          const pmix_value_t *src)
{
    return pmix_bfrops_base_value_xfer(dest, src);
}

PMIX_EXPORT void PMIx_Data_array_destruct(pmix_data_array_t *d)
{
    pmix_bfrops_base_darray_destruct(d);
}

PMIX_EXPORT pmix_status_t PMIx_Info_load(pmix_info_t *info,
                                         const char *key,
                                         const void *data,
                                         pmix_data_type_t type)
{
    if (NULL == key) {
        return PMIX_ERR_BAD_PARAM;
    }
    PMIX_LOAD_KEY(info->key, key);
    info->flags = 0;
    return PMIx_Value_load(&info->value, data, type);
}

PMIX_EXPORT pmix_status_t PMIx_Info_xfer(pmix_info_t *dest,
                                         const pmix_info_t *src)
{
    pmix_status_t rc;

    if (NULL == dest || NULL == src) {
        return PMIX_ERR_BAD_PARAM;
    }
    PMIX_LOAD_KEY(dest->key, src->key);
    dest->flags = src->flags;
    if (PMIX_INFO_IS_PERSISTENT(src)) {
        memcpy(&dest->value, &src->value, sizeof(pmix_value_t));
        rc = PMIX_SUCCESS;
    } else {
        rc = PMIx_Value_xfer(&dest->value, &src->value);
    }
    return rc;
}

void pmix_bfrops_base_value_load(pmix_value_t *v,
                                 const void *data,
                                 pmix_data_type_t type)
{
    pmix_byte_object_t *bo;
    pmix_proc_info_t *pi;
    pmix_envar_t *envar;
    pmix_data_array_t *darray;
    pmix_status_t rc;
    pmix_coord_t *coord;
    pmix_regattr_t *regattr;
    pmix_topology_t *topo;
    pmix_cpuset_t *cpuset;
    pmix_geometry_t *geometry;
    pmix_endpoint_t *endpoint;
    pmix_device_distance_t *devdist;
    pmix_data_buffer_t *dbuf;
    pmix_nspace_t *nspace;
    pmix_proc_stats_t *pstats;
    pmix_disk_stats_t *dkstats;
    pmix_net_stats_t *netstats;
    pmix_node_stats_t *ndstats;

    v->type = type;
    if (NULL == data) {
        /* just set the fields to zero */
        memset(&v->data, 0, sizeof(v->data));
        if (PMIX_BOOL == type) {
            v->data.flag = true; // existence of the attribute indicates true unless specified different
        }
    } else {
        switch (type) {
        case PMIX_UNDEF:
            break;
        case PMIX_BOOL:
            memcpy(&(v->data.flag), data, 1);
            break;
        case PMIX_BYTE:
            memcpy(&(v->data.byte), data, 1);
            break;
        case PMIX_STRING:
            v->data.string = strdup(data);
            break;
        case PMIX_SIZE:
            memcpy(&(v->data.size), data, sizeof(size_t));
            break;
        case PMIX_PID:
            memcpy(&(v->data.pid), data, sizeof(pid_t));
            break;
        case PMIX_INT:
            memcpy(&(v->data.integer), data, sizeof(int));
            break;
        case PMIX_INT8:
            memcpy(&(v->data.int8), data, 1);
            break;
        case PMIX_INT16:
            memcpy(&(v->data.int16), data, 2);
            break;
        case PMIX_INT32:
            memcpy(&(v->data.int32), data, 4);
            break;
        case PMIX_INT64:
            memcpy(&(v->data.int64), data, 8);
            break;
        case PMIX_UINT:
            memcpy(&(v->data.uint), data, sizeof(int));
            break;
        case PMIX_UINT8:
            memcpy(&(v->data.uint8), data, 1);
            break;
        case PMIX_UINT16:
        case PMIX_STOR_ACCESS_TYPE:
            memcpy(&(v->data.uint16), data, 2);
            break;
        case PMIX_UINT32:
            memcpy(&(v->data.uint32), data, 4);
            break;
        case PMIX_UINT64:
        case PMIX_STOR_MEDIUM:
        case PMIX_STOR_ACCESS:
        case PMIX_STOR_PERSIST:
            memcpy(&(v->data.uint64), data, 8);
            break;
        case PMIX_FLOAT:
            memcpy(&(v->data.fval), data, sizeof(float));
            break;
        case PMIX_DOUBLE:
            memcpy(&(v->data.dval), data, sizeof(double));
            break;
        case PMIX_TIMEVAL:
            memcpy(&(v->data.tv), data, sizeof(struct timeval));
            break;
        case PMIX_TIME:
            memcpy(&(v->data.time), data, sizeof(time_t));
            break;
        case PMIX_STATUS:
            memcpy(&(v->data.status), data, sizeof(pmix_status_t));
            break;
        case PMIX_PROC_RANK:
            memcpy(&(v->data.rank), data, sizeof(pmix_rank_t));
            break;
        case PMIX_PROC_NSPACE:
            nspace = (pmix_nspace_t *) data;
            v->data.nspace = (pmix_nspace_t *) malloc(sizeof(pmix_nspace_t));
            PMIX_LOAD_NSPACE(*(v->data.nspace), *nspace);
            break;
        case PMIX_PROC:
            PMIX_PROC_CREATE(v->data.proc, 1);
            if (NULL == v->data.proc) {
                PMIX_ERROR_LOG(PMIX_ERR_NOMEM);
                return;
            }
            memcpy(v->data.proc, data, sizeof(pmix_proc_t));
            break;
        case PMIX_BYTE_OBJECT:
        case PMIX_COMPRESSED_STRING:
        case PMIX_COMPRESSED_BYTE_OBJECT:
            bo = (pmix_byte_object_t *) data;
            v->data.bo.bytes = (char *) malloc(bo->size);
            if (NULL == v->data.bo.bytes) {
                PMIX_ERROR_LOG(PMIX_ERR_NOMEM);
                return;
            }
            memcpy(v->data.bo.bytes, bo->bytes, bo->size);
            memcpy(&(v->data.bo.size), &bo->size, sizeof(size_t));
            break;
        case PMIX_PERSIST:
            memcpy(&(v->data.persist), data, sizeof(pmix_persistence_t));
            break;
        case PMIX_SCOPE:
            memcpy(&(v->data.scope), data, sizeof(pmix_scope_t));
            break;
        case PMIX_DATA_RANGE:
            memcpy(&(v->data.range), data, sizeof(pmix_data_range_t));
            break;
        case PMIX_PROC_STATE:
            memcpy(&(v->data.state), data, sizeof(pmix_proc_state_t));
            break;
        case PMIX_PROC_INFO:
            PMIX_PROC_INFO_CREATE(v->data.pinfo, 1);
            if (NULL == v->data.pinfo) {
                PMIX_ERROR_LOG(PMIX_ERR_NOMEM);
                return;
            }
            pi = (pmix_proc_info_t *) data;
            memcpy(&(v->data.pinfo->proc), &pi->proc, sizeof(pmix_proc_t));
            if (NULL != pi->hostname) {
                v->data.pinfo->hostname = strdup(pi->hostname);
            }
            if (NULL != pi->executable_name) {
                v->data.pinfo->executable_name = strdup(pi->executable_name);
            }
            memcpy(&(v->data.pinfo->pid), &pi->pid, sizeof(pid_t));
            memcpy(&(v->data.pinfo->exit_code), &pi->exit_code, sizeof(int));
            break;
        case PMIX_DATA_ARRAY:
            darray = (pmix_data_array_t *) data;
            rc = pmix_bfrops_base_copy_darray(&v->data.darray, darray, PMIX_DATA_ARRAY);
            if (PMIX_SUCCESS != rc) {
                PMIX_ERROR_LOG(rc);
            }
            break;
        case PMIX_POINTER:
            v->data.ptr = (void *) data;
            break;
        case PMIX_ALLOC_DIRECTIVE:
            memcpy(&(v->data.adir), data, sizeof(pmix_alloc_directive_t));
            break;
        case PMIX_ENVAR:
            envar = (pmix_envar_t *) data;
            if (NULL != envar->envar) {
                v->data.envar.envar = strdup(envar->envar);
            }
            if (NULL != envar->value) {
                v->data.envar.value = strdup(envar->value);
            }
            v->data.envar.separator = envar->separator;
            break;
        case PMIX_COORD:
            coord = (pmix_coord_t *) data;
            rc = pmix_bfrops_base_copy_coord(&v->data.coord, coord, PMIX_COORD);
            if (PMIX_SUCCESS != rc) {
                PMIX_ERROR_LOG(rc);
            }
            break;
        case PMIX_LINK_STATE:
            memcpy(&(v->data.linkstate), data, sizeof(pmix_link_state_t));
            break;
        case PMIX_JOB_STATE:
            memcpy(&(v->data.jstate), data, sizeof(pmix_job_state_t));
            break;
        case PMIX_TOPO:
            topo = (pmix_topology_t *) data;
            rc = pmix_bfrops_base_copy_topology(&v->data.topo, topo, PMIX_TOPO);
            if (PMIX_ERR_INIT == rc || PMIX_ERR_NOT_SUPPORTED == rc) {
                /* we are not initialized yet, so just copy the pointer */
                v->data.topo = topo;
                rc = PMIX_SUCCESS;
            }
            break;
        case PMIX_PROC_CPUSET:
            cpuset = (pmix_cpuset_t *) data;
            rc = pmix_bfrops_base_copy_cpuset(&v->data.cpuset, cpuset, PMIX_PROC_CPUSET);
            if (PMIX_ERR_INIT == rc || PMIX_ERR_NOT_SUPPORTED == rc) {
                /* we are not initialized yet, so just copy the pointer */
                v->data.cpuset = cpuset;
                rc = PMIX_SUCCESS;
            }
            break;
        case PMIX_LOCTYPE:
            memcpy(&(v->data.locality), data, sizeof(pmix_locality_t));
            break;
        case PMIX_GEOMETRY:
            geometry = (pmix_geometry_t *) data;
            rc = pmix_bfrops_base_copy_geometry(&v->data.geometry, geometry, PMIX_GEOMETRY);
            if (PMIX_SUCCESS != rc) {
                PMIX_ERROR_LOG(rc);
            }
            break;
        case PMIX_DEVTYPE:
            memcpy(&(v->data.devtype), data, sizeof(pmix_device_type_t));
            break;
        case PMIX_DEVICE_DIST:
            devdist = (pmix_device_distance_t *) data;
            rc = pmix_bfrops_base_copy_devdist(&v->data.devdist, devdist, PMIX_DEVICE_DIST);
            if (PMIX_SUCCESS != rc) {
                PMIX_ERROR_LOG(rc);
            }
            break;
        case PMIX_ENDPOINT:
            endpoint = (pmix_endpoint_t *) data;
            rc = pmix_bfrops_base_copy_endpoint(&v->data.endpoint, endpoint, PMIX_ENDPOINT);
            if (PMIX_SUCCESS != rc) {
                PMIX_ERROR_LOG(rc);
            }
            break;
        case PMIX_REGATTR:
            regattr = (pmix_regattr_t *) data;
            rc = pmix_bfrops_base_copy_regattr((pmix_regattr_t **) &v->data.ptr, regattr,
                                               PMIX_REGATTR);
            if (PMIX_SUCCESS != rc) {
                PMIX_ERROR_LOG(rc);
            }
            break;
        case PMIX_REGEX:
            /* load it into the byte object */
            rc = pmix_preg.copy(&v->data.bo.bytes, &v->data.bo.size, (char *) data);
            if (PMIX_SUCCESS != rc) {
                PMIX_ERROR_LOG(rc);
            }
            break;
        case PMIX_DATA_BUFFER:
            dbuf = (pmix_data_buffer_t *) data;
            PMIX_DATA_BUFFER_CREATE(v->data.dbuf);
            rc = PMIx_Data_copy_payload(v->data.dbuf, dbuf);
            if (PMIX_SUCCESS != rc) {
                PMIX_ERROR_LOG(rc);
            }
            break;
        case PMIX_PROC_STATS:
            pstats = (pmix_proc_stats_t *) data;
            rc = pmix_bfrops_base_copy_pstats(&v->data.pstats, pstats, PMIX_PROC_STATS);
            if (PMIX_SUCCESS != rc) {
                PMIX_ERROR_LOG(rc);
            }
            break;
        case PMIX_DISK_STATS:
            dkstats = (pmix_disk_stats_t *) data;
            rc = pmix_bfrops_base_copy_dkstats(&v->data.dkstats, dkstats, PMIX_DISK_STATS);
            if (PMIX_SUCCESS != rc) {
                PMIX_ERROR_LOG(rc);
            }
            break;
        case PMIX_NET_STATS:
            netstats = (pmix_net_stats_t *) data;
            rc = pmix_bfrops_base_copy_netstats(&v->data.netstats, netstats, PMIX_NET_STATS);
            if (PMIX_SUCCESS != rc) {
                PMIX_ERROR_LOG(rc);
            }
            break;
        case PMIX_NODE_STATS:
            ndstats = (pmix_node_stats_t *) data;
            rc = pmix_bfrops_base_copy_ndstats(&v->data.ndstats, ndstats, PMIX_NODE_STATS);
            if (PMIX_SUCCESS != rc) {
                PMIX_ERROR_LOG(rc);
            }
            break;

        default:
            /* silence warnings */
            break;
        }
    }
    return;
}

pmix_status_t pmix_bfrops_base_value_unload(pmix_value_t *kv, void **data, size_t *sz)
{
    pmix_status_t rc;
    pmix_envar_t *envar;
    pmix_data_array_t **darray;
    pmix_regattr_t *regattr, *r;

    rc = PMIX_SUCCESS;
    if (NULL == data ||
        (NULL == *data && PMIX_STRING != kv->type && PMIX_BYTE_OBJECT != kv->type)) {
        rc = PMIX_ERR_BAD_PARAM;
    } else {
        switch (kv->type) {
        case PMIX_UNDEF:
            rc = PMIX_ERR_UNKNOWN_DATA_TYPE;
            break;
        case PMIX_BOOL:
            memcpy(*data, &(kv->data.flag), sizeof(bool));
            *sz = sizeof(bool);
            break;
        case PMIX_BYTE:
            memcpy(*data, &(kv->data.byte), sizeof(uint8_t));
            *sz = sizeof(uint8_t);
            break;
        case PMIX_STRING:
            if (NULL != kv->data.string) {
                *data = strdup(kv->data.string);
                *sz = strlen(kv->data.string);
            }
            break;
        case PMIX_SIZE:
            memcpy(*data, &(kv->data.size), sizeof(size_t));
            *sz = sizeof(size_t);
            break;
        case PMIX_PID:
            memcpy(*data, &(kv->data.pid), sizeof(pid_t));
            *sz = sizeof(pid_t);
            break;
        case PMIX_UINT:
        case PMIX_INT:
            memcpy(*data, &(kv->data.integer), sizeof(int));
            *sz = sizeof(int);
            break;
        case PMIX_UINT8:
        case PMIX_INT8:
            memcpy(*data, &(kv->data.int8), sizeof(int8_t));
            *sz = sizeof(int8_t);
            break;
        case PMIX_UINT16:
        case PMIX_INT16:
        case PMIX_STOR_ACCESS_TYPE:
            memcpy(*data, &(kv->data.int16), sizeof(int16_t));
            *sz = sizeof(int16_t);
            break;
        case PMIX_UINT32:
        case PMIX_INT32:
            memcpy(*data, &(kv->data.int32), sizeof(int32_t));
            *sz = sizeof(int32_t);
            break;
        case PMIX_UINT64:
        case PMIX_INT64:
        case PMIX_STOR_MEDIUM:
        case PMIX_STOR_ACCESS:
        case PMIX_STOR_PERSIST:
            memcpy(*data, &(kv->data.int64), sizeof(int64_t));
            *sz = sizeof(int64_t);
            break;
        case PMIX_FLOAT:
            memcpy(*data, &(kv->data.fval), sizeof(float));
            *sz = sizeof(float);
            break;
        case PMIX_DOUBLE:
            memcpy(*data, &(kv->data.dval), sizeof(double));
            *sz = sizeof(double);
            break;
        case PMIX_TIMEVAL:
            memcpy(*data, &(kv->data.tv), sizeof(struct timeval));
            *sz = sizeof(struct timeval);
            break;
        case PMIX_TIME:
            memcpy(*data, &(kv->data.time), sizeof(time_t));
            *sz = sizeof(time_t);
            break;
        case PMIX_STATUS:
            memcpy(*data, &(kv->data.status), sizeof(pmix_status_t));
            *sz = sizeof(pmix_status_t);
            break;
        case PMIX_PROC_RANK:
            memcpy(*data, &(kv->data.rank), sizeof(pmix_rank_t));
            *sz = sizeof(pmix_rank_t);
            break;
        case PMIX_PROC_NSPACE:
            PMIX_LOAD_NSPACE(*data, *kv->data.nspace);
            *sz = strlen(*kv->data.nspace);
            break;
        case PMIX_PROC:
            PMIX_XFER_PROCID(*data, kv->data.proc);
            *sz = sizeof(pmix_proc_t);
            break;
        case PMIX_BYTE_OBJECT:
        case PMIX_COMPRESSED_STRING:
        case PMIX_COMPRESSED_BYTE_OBJECT:
            if (NULL != kv->data.bo.bytes && 0 < kv->data.bo.size) {
                *data = kv->data.bo.bytes;
                *sz = kv->data.bo.size;

            } else {
                *data = NULL;
                *sz = 0;
            }
            break;
        case PMIX_PERSIST:
            memcpy(*data, &(kv->data.persist), sizeof(pmix_persistence_t));
            *sz = sizeof(pmix_persistence_t);
            break;
        case PMIX_SCOPE:
            memcpy(*data, &(kv->data.scope), sizeof(pmix_scope_t));
            *sz = sizeof(pmix_scope_t);
            break;
        case PMIX_DATA_RANGE:
            memcpy(*data, &(kv->data.range), sizeof(pmix_data_range_t));
            *sz = sizeof(pmix_data_range_t);
            break;
        case PMIX_PROC_STATE:
            memcpy(*data, &(kv->data.state), sizeof(pmix_proc_state_t));
            *sz = sizeof(pmix_proc_state_t);
            break;
        case PMIX_PROC_INFO:
            rc = pmix_bfrops_base_copy_pinfo((pmix_proc_info_t **) data, kv->data.pinfo,
                                             PMIX_PROC_INFO);
            if (PMIX_SUCCESS == rc) {
                *sz = sizeof(pmix_proc_info_t);
            }
            break;
        case PMIX_DATA_ARRAY:
            darray = (pmix_data_array_t **) data;
            rc = pmix_bfrops_base_copy_darray(darray, kv->data.darray, PMIX_DATA_ARRAY);
            if (PMIX_SUCCESS == rc) {
                *sz = sizeof(pmix_data_array_t);
            }
            break;
        case PMIX_POINTER:
            *data = (void *) kv->data.ptr;
            *sz = sizeof(void *);
            break;
        case PMIX_ALLOC_DIRECTIVE:
            memcpy(*data, &(kv->data.adir), sizeof(pmix_alloc_directive_t));
            *sz = sizeof(pmix_alloc_directive_t);
            break;
        case PMIX_ENVAR:
            PMIX_ENVAR_CREATE(envar, 1);
            if (NULL == envar) {
                return PMIX_ERR_NOMEM;
            }
            if (NULL != kv->data.envar.envar) {
                envar->envar = strdup(kv->data.envar.envar);
            }
            if (NULL != kv->data.envar.value) {
                envar->value = strdup(kv->data.envar.value);
            }
            envar->separator = kv->data.envar.separator;
            *data = envar;
            *sz = sizeof(pmix_envar_t);
            break;
        case PMIX_COORD:
            rc = pmix_bfrops_base_copy_coord((pmix_coord_t **) data, kv->data.coord, PMIX_COORD);
            if (PMIX_SUCCESS == rc) {
                *sz = sizeof(pmix_coord_t);
            }
            break;
        case PMIX_LINK_STATE:
            memcpy(*data, &(kv->data.linkstate), sizeof(pmix_link_state_t));
            *sz = sizeof(pmix_link_state_t);
            break;
        case PMIX_JOB_STATE:
            memcpy(*data, &(kv->data.jstate), sizeof(pmix_job_state_t));
            *sz = sizeof(pmix_job_state_t);
            break;
        case PMIX_TOPO:
            rc = pmix_bfrops_base_copy_topology((pmix_topology_t **) data, kv->data.topo,
                                                PMIX_TOPO);
            if (PMIX_SUCCESS == rc) {
                *sz = sizeof(pmix_topology_t);
            } else if (PMIX_ERR_INIT == rc || PMIX_ERR_NOT_SUPPORTED == rc) {
                *data = malloc(sizeof(pmix_topology_t));
                memcpy(*data, kv->data.topo, sizeof(pmix_topology_t));
                *sz = sizeof(pmix_topology_t);
                rc = PMIX_SUCCESS;
            }
            break;
        case PMIX_PROC_CPUSET:
            rc = pmix_bfrops_base_copy_cpuset((pmix_cpuset_t **) data, kv->data.cpuset,
                                              PMIX_PROC_CPUSET);
            if (PMIX_SUCCESS == rc) {
                *sz = sizeof(pmix_cpuset_t);
            } else if (PMIX_ERR_INIT == rc || PMIX_ERR_NOT_SUPPORTED == rc) {
                *data = malloc(sizeof(pmix_cpuset_t));
                memcpy(*data, kv->data.cpuset, sizeof(pmix_cpuset_t));
                *sz = sizeof(pmix_cpuset_t);
                rc = PMIX_SUCCESS;
            }
            break;
        case PMIX_LOCTYPE:
            memcpy(*data, &(kv->data.locality), sizeof(pmix_locality_t));
            *sz = sizeof(pmix_locality_t);
            break;
        case PMIX_GEOMETRY:
            rc = pmix_bfrops_base_copy_geometry((pmix_geometry_t **) data, kv->data.geometry,
                                                PMIX_GEOMETRY);
            if (PMIX_SUCCESS == rc) {
                *sz = sizeof(pmix_geometry_t);
            }
            break;
        case PMIX_DEVTYPE:
            memcpy(*data, &(kv->data.devtype), sizeof(pmix_device_type_t));
            *sz = sizeof(pmix_device_type_t);
            break;
        case PMIX_DEVICE_DIST:
            rc = pmix_bfrops_base_copy_devdist((pmix_device_distance_t **) data, kv->data.devdist,
                                               PMIX_DEVICE_DIST);
            if (PMIX_SUCCESS == rc) {
                *sz = sizeof(pmix_device_distance_t);
            }
            break;
        case PMIX_ENDPOINT:
            rc = pmix_bfrops_base_copy_endpoint((pmix_endpoint_t **) data, kv->data.endpoint,
                                                PMIX_ENDPOINT);
            if (PMIX_SUCCESS == rc) {
                *sz = sizeof(pmix_endpoint_t);
            }
            break;
        case PMIX_REGATTR:
            PMIX_REGATTR_CREATE(regattr, 1);
            if (NULL == regattr) {
                return PMIX_ERR_NOMEM;
            }
            r = (pmix_regattr_t *) kv->data.ptr;
            if (NULL != r->name) {
                regattr->name = strdup(r->name);
            }
            PMIX_LOAD_KEY(regattr->string, r->string);
            regattr->type = r->type;
            regattr->description = pmix_argv_copy(r->description);
            *data = regattr;
            *sz = sizeof(pmix_regattr_t);
            break;
        case PMIX_REGEX:
            if (NULL != kv->data.bo.bytes && 0 < kv->data.bo.size) {
                *data = kv->data.bo.bytes;
                *sz = kv->data.bo.size;
            } else {
                *data = NULL;
                *sz = 0;
            }
            break;
        case PMIX_DATA_BUFFER:
            rc = pmix_bfrops_base_copy_dbuf((pmix_data_buffer_t **) data, kv->data.dbuf,
                                            PMIX_DATA_BUFFER);
            if (PMIX_SUCCESS == rc) {
                *sz = sizeof(pmix_data_buffer_t);
            }
            break;
        case PMIX_PROC_STATS:
            rc = pmix_bfrops_base_copy_pstats((pmix_proc_stats_t **) data, kv->data.pstats,
                                              PMIX_PROC_STATS);
            if (PMIX_SUCCESS == rc) {
                *sz = sizeof(pmix_proc_stats_t);
            }
            break;
        case PMIX_DISK_STATS:
            rc = pmix_bfrops_base_copy_dkstats((pmix_disk_stats_t **) data, kv->data.dkstats,
                                               PMIX_DISK_STATS);
            if (PMIX_SUCCESS == rc) {
                *sz = sizeof(pmix_disk_stats_t);
            }
            break;
        case PMIX_NET_STATS:
            rc = pmix_bfrops_base_copy_netstats((pmix_net_stats_t **) data, kv->data.netstats,
                                                PMIX_NET_STATS);
            if (PMIX_SUCCESS == rc) {
                *sz = sizeof(pmix_net_stats_t);
            }
            break;
        case PMIX_NODE_STATS:
            rc = pmix_bfrops_base_copy_ndstats((pmix_node_stats_t **) data, kv->data.ndstats,
                                               PMIX_NODE_STATS);
            if (PMIX_SUCCESS == rc) {
                *sz = sizeof(pmix_node_stats_t);
            }
            break;
        default:
            /* silence warnings */
            rc = PMIX_ERROR;
            break;
        }
    }
    return rc;
}

void pmix_bfrops_base_darray_destruct(pmix_data_array_t *d)
{
    size_t n;
    char **s;
    pmix_value_t *vt;
    pmix_app_t *ap;
    pmix_info_t *info;
    pmix_pdata_t *pd;
    pmix_buffer_t *pb;
    pmix_byte_object_t *bo;
    pmix_kval_t *kv;
    pmix_proc_info_t *pi;
    pmix_data_array_t *darray;
    pmix_query_t *q;
    pmix_envar_t *ev;
    pmix_coord_t *coord;
    pmix_regattr_t *reg;
    pmix_cpuset_t *cp;
    pmix_topology_t *topo;
    pmix_geometry_t *geo;
    pmix_device_distance_t *dvd;
    pmix_endpoint_t *endpt;
    pmix_data_buffer_t *db;
    pmix_proc_stats_t *pstats;
    pmix_disk_stats_t *dkstats;
    pmix_net_stats_t *netstats;
    pmix_node_stats_t *ndstats;

    switch (d->type) {
        case PMIX_STRING:
            s = (char**)d->array;
            for (n=0; n < d->size; n++) {
                if (NULL != s[n]) {
                    pmix_free(s[n]);
                }
            }
            pmix_free(d->array);
            break;
        case PMIX_VALUE:
            vt = (pmix_value_t*)d->array;
            PMIX_VALUE_FREE(vt, d->size);
            break;
        case PMIX_APP:
            ap = (pmix_app_t*)d->array;
            PMIX_APP_FREE(ap, d->size);
            break;
        case PMIX_INFO:
            info = (pmix_info_t*)d->array;
            PMIX_INFO_FREE(info, d->size);
            break;
        case PMIX_PDATA:
            pd = (pmix_pdata_t*)d->array;
            PMIX_PDATA_FREE(pd, d->size);
            break;
        case PMIX_BUFFER:
            pb = (pmix_buffer_t*)d->array;
            for (n=0; n < d->size; n++) {
                PMIX_DESTRUCT(&pb[n]);
            }
            pmix_free(d->array);
            break;
        case PMIX_BYTE_OBJECT:
        case PMIX_COMPRESSED_STRING:
        case PMIX_COMPRESSED_BYTE_OBJECT:
            bo = (pmix_byte_object_t*)d->array;
            for (n=0; n < d->size; n++) {
                if (NULL != bo[n].bytes) {
                    pmix_free(bo[n].bytes);
                }
            }
            pmix_free(d->array);
            break;
        case PMIX_KVAL:
            kv = (pmix_kval_t*)d->array;
            for (n=0; n < d->size; n++) {
                if (NULL != kv[n].key) {
                    pmix_free(kv[n].key);
                }
                if (NULL != kv[n].value) {
                    PMIX_VALUE_FREE(kv[n].value, 1);
                }
            }
            pmix_free(d->array);
            break;
        case PMIX_PROC_INFO:
            pi = (pmix_proc_info_t*)d->array;
            PMIX_PROC_INFO_FREE(pi, d->size);
            break;
        case PMIX_DATA_ARRAY:
            darray = (pmix_data_array_t *) d->array;
            pmix_bfrops_base_darray_destruct(darray);
            break;
        case PMIX_QUERY:
            q = (pmix_query_t*)d->array;
            PMIX_QUERY_FREE(q, d->size);
            break;
        case PMIX_ENVAR:
            ev = (pmix_envar_t*)d->array;
            PMIX_ENVAR_FREE(ev, d->size);
            break;
        case PMIX_COORD:
            coord = (pmix_coord_t*)d->array;
            PMIX_COORD_FREE(coord, d->size);
            break;
        case PMIX_REGATTR:
            reg = (pmix_regattr_t*)d->array;
            PMIX_REGATTR_FREE(reg, d->size);
            break;
        case PMIX_PROC_CPUSET:
            cp = (pmix_cpuset_t*)d->array;
            pmix_hwloc_release_cpuset(cp, d->size);
            break;
        case PMIX_TOPO:
            topo = (pmix_topology_t*)d->array;
            pmix_hwloc_release_topology(topo, d->size);
            break;
        case PMIX_GEOMETRY:
            geo = (pmix_geometry_t*)d->array;
            PMIX_GEOMETRY_FREE(geo, d->size);
            break;
        case PMIX_DEVICE_DIST:
            dvd = (pmix_device_distance_t*)d->array;
            PMIX_DEVICE_DIST_FREE(dvd, d->size);
            break;
        case PMIX_ENDPOINT:
            endpt = (pmix_endpoint_t*)d->array;
            PMIX_ENDPOINT_FREE(endpt, d->size);
            break;
        case PMIX_REGEX:
            bo = (pmix_byte_object_t*)d->array;
            for (n=0; n < d->size; n++) {
                if (NULL != bo[n].bytes) {
                    pmix_preg.release(bo[n].bytes);
                }
            }
            pmix_free(d->array);
            break;
        case PMIX_DATA_BUFFER:
            db = (pmix_data_buffer_t*)d->array;
            for (n=0; n < d->size; n++) {
                PMIX_DATA_BUFFER_DESTRUCT(&db[n]);
            }
            pmix_free(d->array);
            break;
        case PMIX_PROC_STATS:
            pstats = (pmix_proc_stats_t*)d->array;
            PMIX_PROC_STATS_FREE(pstats, d->size);
            break;
        case PMIX_DISK_STATS:
            dkstats = (pmix_disk_stats_t*)d->array;
            PMIX_DISK_STATS_FREE(dkstats, d->size);
            break;
        case PMIX_NET_STATS:
            netstats = (pmix_net_stats_t*)d->array;
            PMIX_NET_STATS_FREE(netstats, d->size);
            break;
        case PMIX_NODE_STATS:
            ndstats = (pmix_node_stats_t*)d->array;
            PMIX_NODE_STATS_FREE(ndstats, d->size);
            break;

        default:
            if (NULL != d->array) {
                pmix_free(d->array);
            }
            break;
    }
    d->array = NULL;
    d->type = PMIX_UNDEF;
    d->size = 0;
    return;
}

void pmix_bfrops_base_value_destruct(pmix_value_t *v)
{
    switch (v->type) {
        case PMIX_STRING:
            if (NULL != v->data.string) {
                free(v->data.string);
            }
            break;
        case PMIX_PROC:
            if (NULL != v->data.proc) {
                PMIX_PROC_FREE(v->data.proc, 1);
            }
            break;
        case PMIX_BYTE_OBJECT:
        case PMIX_COMPRESSED_STRING:
        case PMIX_COMPRESSED_BYTE_OBJECT:
            if (NULL != v->data.bo.bytes) {
                free(v->data.bo.bytes);
            }
            break;
        case PMIX_PROC_INFO:
            if (NULL != v->data.pinfo) {
                PMIX_PROC_INFO_FREE(v->data.pinfo, 1);
            }
            break;
        case PMIX_DATA_ARRAY:
            if (NULL != v->data.darray) {
                pmix_bfrops_base_darray_destruct(v->data.darray);
            }
            break;
        case PMIX_ENVAR:
            if (NULL != v->data.envar.envar) {
                free(v->data.envar.envar);
            }
            if (NULL != v->data.envar.value) {
                free(v->data.envar.value);
            }
            break;
        case PMIX_COORD:
            if (NULL != v->data.coord) {
                PMIX_COORD_FREE(v->data.coord, 1);
            }
            break;
        case PMIX_TOPO:
            if (NULL != v->data.topo) {
                pmix_hwloc_release_topology(v->data.topo, 1);
            }
            break;
        case PMIX_PROC_CPUSET:
            if (NULL != v->data.cpuset) {
                pmix_hwloc_release_cpuset(v->data.cpuset, 1);
            }
            break;
        case PMIX_GEOMETRY:
            if (NULL != v->data.geometry) {
                PMIX_GEOMETRY_FREE(v->data.geometry, 1);
            }
            break;
        case PMIX_DEVICE_DIST:
            if (NULL != v->data.devdist) {
                PMIX_DEVICE_DIST_DESTRUCT(v->data.devdist);
            }
            break;
        case PMIX_ENDPOINT:
            if (NULL != v->data.endpoint) {
                PMIX_ENDPOINT_DESTRUCT(v->data.endpoint);
            }
            break;
        case PMIX_REGATTR:
            if (NULL != v->data.ptr) {
                PMIX_REGATTR_DESTRUCT((pmix_regattr_t*)v->data.ptr);
            }
            break;
        case PMIX_REGEX:
            if (NULL != v->data.bo.bytes) {
                pmix_preg.release(v->data.bo.bytes);
            }
            break;
        case PMIX_DATA_BUFFER:
            if (NULL != v->data.dbuf) {
                PMIX_DATA_BUFFER_RELEASE(v->data.dbuf);
            }
            break;
        case PMIX_PROC_STATS:
            if (NULL != v->data.pstats) {
                PMIX_PROC_STATS_RELEASE(v->data.pstats);
            }
            break;
        case PMIX_DISK_STATS:
            if (NULL != v->data.dkstats) {
                PMIX_DISK_STATS_RELEASE(v->data.dkstats);
            }
            break;
        case PMIX_NET_STATS:
            if (NULL != v->data.netstats) {
                PMIX_NET_STATS_RELEASE(v->data.netstats);
            }
            break;
        case PMIX_NODE_STATS:
            if (NULL != v->data.ndstats) {
                PMIX_NODE_STATS_RELEASE(v->data.ndstats);
            }
            break;

        default:
            /* silence warnings */
            break;
    }
    // mark the value as no longer defined
    memset(v, 0, sizeof(pmix_value_t));
    v->type = PMIX_UNDEF;
    return;
}

/* Xfer FUNCTIONS FOR GENERIC PMIX TYPES */
pmix_status_t pmix_bfrops_base_value_xfer(pmix_value_t *p, const pmix_value_t *src)
{
    pmix_status_t rc;

    /* copy the right field */
    p->type = src->type;
    switch (src->type) {
    case PMIX_UNDEF:
        break;
    case PMIX_BOOL:
        p->data.flag = src->data.flag;
        break;
    case PMIX_BYTE:
        p->data.byte = src->data.byte;
        break;
    case PMIX_STRING:
        if (NULL != src->data.string) {
            p->data.string = strdup(src->data.string);
        } else {
            p->data.string = NULL;
        }
        break;
    case PMIX_SIZE:
        p->data.size = src->data.size;
        break;
    case PMIX_PID:
        p->data.pid = src->data.pid;
        break;
    case PMIX_INT:
        /* to avoid alignment issues */
        memcpy(&p->data.integer, &src->data.integer, sizeof(int));
        break;
    case PMIX_INT8:
        p->data.int8 = src->data.int8;
        break;
    case PMIX_INT16:
        /* to avoid alignment issues */
        memcpy(&p->data.int16, &src->data.int16, 2);
        break;
    case PMIX_INT32:
        /* to avoid alignment issues */
        memcpy(&p->data.int32, &src->data.int32, 4);
        break;
    case PMIX_INT64:
        /* to avoid alignment issues */
        memcpy(&p->data.int64, &src->data.int64, 8);
        break;
    case PMIX_UINT:
        /* to avoid alignment issues */
        memcpy(&p->data.uint, &src->data.uint, sizeof(unsigned int));
        break;
    case PMIX_UINT8:
        p->data.uint8 = src->data.uint8;
        break;
    case PMIX_UINT16:
    case PMIX_STOR_ACCESS_TYPE:
        /* to avoid alignment issues */
        memcpy(&p->data.uint16, &src->data.uint16, 2);
        break;
    case PMIX_UINT32:
        /* to avoid alignment issues */
        memcpy(&p->data.uint32, &src->data.uint32, 4);
        break;
    case PMIX_UINT64:
    case PMIX_STOR_MEDIUM:
    case PMIX_STOR_ACCESS:
    case PMIX_STOR_PERSIST:
       /* to avoid alignment issues */
        memcpy(&p->data.uint64, &src->data.uint64, 8);
        break;
    case PMIX_FLOAT:
        p->data.fval = src->data.fval;
        break;
    case PMIX_DOUBLE:
        p->data.dval = src->data.dval;
        break;
    case PMIX_TIMEVAL:
        memcpy(&p->data.tv, &src->data.tv, sizeof(struct timeval));
        break;
    case PMIX_TIME:
        memcpy(&p->data.time, &src->data.time, sizeof(time_t));
        break;
    case PMIX_STATUS:
        memcpy(&p->data.status, &src->data.status, sizeof(pmix_status_t));
        break;
    case PMIX_PROC_RANK:
        memcpy(&p->data.rank, &src->data.rank, sizeof(pmix_rank_t));
        break;
    case PMIX_PROC_NSPACE:
        return pmix_bfrops_base_copy_nspace(&p->data.nspace, src->data.nspace, PMIX_PROC_NSPACE);
    case PMIX_PROC:
        PMIX_PROC_CREATE(p->data.proc, 1);
        if (NULL == p->data.proc) {
            return PMIX_ERR_NOMEM;
        }
        memcpy(p->data.proc, src->data.proc, sizeof(pmix_proc_t));
        break;
    case PMIX_BYTE_OBJECT:
    case PMIX_COMPRESSED_STRING:
    case PMIX_REGEX:
    case PMIX_COMPRESSED_BYTE_OBJECT:
        memset(&p->data.bo, 0, sizeof(pmix_byte_object_t));
        if (NULL != src->data.bo.bytes && 0 < src->data.bo.size) {
            p->data.bo.bytes = malloc(src->data.bo.size);
            memcpy(p->data.bo.bytes, src->data.bo.bytes, src->data.bo.size);
            p->data.bo.size = src->data.bo.size;
        } else {
            p->data.bo.bytes = NULL;
            p->data.bo.size = 0;
        }
        break;
    case PMIX_PERSIST:
        memcpy(&p->data.persist, &src->data.persist, sizeof(pmix_persistence_t));
        break;
    case PMIX_SCOPE:
        memcpy(&p->data.scope, &src->data.scope, sizeof(pmix_scope_t));
        break;
    case PMIX_DATA_RANGE:
        memcpy(&p->data.range, &src->data.range, sizeof(pmix_data_range_t));
        break;
    case PMIX_PROC_STATE:
        memcpy(&p->data.state, &src->data.state, sizeof(pmix_proc_state_t));
        break;
    case PMIX_PROC_INFO:
        return pmix_bfrops_base_copy_pinfo(&p->data.pinfo, src->data.pinfo, PMIX_PROC_INFO);
    case PMIX_DATA_ARRAY:
        return pmix_bfrops_base_copy_darray(&p->data.darray, src->data.darray, PMIX_DATA_ARRAY);
    case PMIX_POINTER:
        p->data.ptr = src->data.ptr;
        break;
    case PMIX_ALLOC_DIRECTIVE:
        memcpy(&p->data.adir, &src->data.adir, sizeof(pmix_alloc_directive_t));
        break;
    case PMIX_ENVAR:
        PMIX_ENVAR_CONSTRUCT(&p->data.envar);
        if (NULL != src->data.envar.envar) {
            p->data.envar.envar = strdup(src->data.envar.envar);
        }
        if (NULL != src->data.envar.value) {
            p->data.envar.value = strdup(src->data.envar.value);
        }
        p->data.envar.separator = src->data.envar.separator;
        break;
    case PMIX_COORD:
        return pmix_bfrops_base_copy_coord(&p->data.coord, src->data.coord, PMIX_COORD);
    case PMIX_LINK_STATE:
        memcpy(&p->data.linkstate, &src->data.linkstate, sizeof(pmix_link_state_t));
        break;
    case PMIX_JOB_STATE:
        memcpy(&p->data.jstate, &src->data.jstate, sizeof(pmix_job_state_t));
        break;
    case PMIX_TOPO:
        rc = pmix_bfrops_base_copy_topology(&p->data.topo, src->data.topo, PMIX_TOPO);
        if (PMIX_ERR_INIT == rc || PMIX_ERR_NOT_SUPPORTED == rc) {
            /* we are being asked to do this before init, so
             * just copy the pointer across */
            p->data.topo = src->data.topo;
        }
        break;
    case PMIX_PROC_CPUSET:
        rc = pmix_bfrops_base_copy_cpuset(&p->data.cpuset, src->data.cpuset, PMIX_PROC_CPUSET);
        if (PMIX_ERR_INIT == rc || PMIX_ERR_NOT_SUPPORTED == rc) {
            /* we are being asked to do this before init, so
             * just copy the pointer across */
            p->data.cpuset = src->data.cpuset;
        }
        break;
    case PMIX_LOCTYPE:
        memcpy(&p->data.locality, &src->data.locality, sizeof(pmix_locality_t));
        break;
    case PMIX_GEOMETRY:
        return pmix_bfrops_base_copy_geometry(&p->data.geometry, src->data.geometry, PMIX_GEOMETRY);
    case PMIX_DEVTYPE:
        memcpy(&p->data.devtype, &src->data.devtype, sizeof(pmix_device_type_t));
        break;
    case PMIX_DEVICE_DIST:
        return pmix_bfrops_base_copy_devdist(&p->data.devdist, src->data.devdist, PMIX_DEVICE_DIST);
    case PMIX_ENDPOINT:
        return pmix_bfrops_base_copy_endpoint(&p->data.endpoint, src->data.endpoint, PMIX_ENDPOINT);
    case PMIX_REGATTR:
        return pmix_bfrops_base_copy_regattr((pmix_regattr_t **) &p->data.ptr, src->data.ptr,
                                             PMIX_REGATTR);
    case PMIX_DATA_BUFFER:
        return pmix_bfrops_base_copy_dbuf(&p->data.dbuf, src->data.dbuf, PMIX_DATA_BUFFER);
    case PMIX_PROC_STATS:
        return pmix_bfrops_base_copy_pstats(&p->data.pstats, src->data.pstats, PMIX_PROC_STATS);
    case PMIX_DISK_STATS:
        return pmix_bfrops_base_copy_dkstats(&p->data.dkstats, src->data.dkstats, PMIX_DISK_STATS);
    case PMIX_NET_STATS:
        return pmix_bfrops_base_copy_netstats(&p->data.netstats, src->data.netstats,
                                              PMIX_NET_STATS);
    case PMIX_NODE_STATS:
        return pmix_bfrops_base_copy_ndstats(&p->data.ndstats, src->data.ndstats, PMIX_NODE_STATS);
    default:
        pmix_output(0, "PMIX-XFER-VALUE: UNSUPPORTED TYPE %d", (int) src->type);
        return PMIX_ERROR;
    }
    return PMIX_SUCCESS;
}

/**
 * Internal function that resizes (expands) an inuse buffer if
 * necessary.
 */
char *pmix_bfrop_buffer_extend(pmix_buffer_t *buffer, size_t bytes_to_add)
{
    size_t required, to_alloc;
    size_t pack_offset, unpack_offset;

    /* Check to see if we have enough space already */
    if (0 == bytes_to_add) {
        return buffer->pack_ptr;
    }

    if ((buffer->bytes_allocated - buffer->bytes_used) >= bytes_to_add) {
        return buffer->pack_ptr;
    }

    required = buffer->bytes_used + bytes_to_add;
    if (required >= pmix_bfrops_globals.threshold_size) {
        to_alloc = ((required + pmix_bfrops_globals.threshold_size - 1)
                    / pmix_bfrops_globals.threshold_size)
                   * pmix_bfrops_globals.threshold_size;
    } else {
        to_alloc = buffer->bytes_allocated;
        if (0 == to_alloc) {
            to_alloc = pmix_bfrops_globals.initial_size;
        }
        while (to_alloc < required) {
            to_alloc <<= 1;
        }
    }

    if (NULL != buffer->base_ptr) {
        pack_offset = ((char *) buffer->pack_ptr) - ((char *) buffer->base_ptr);
        unpack_offset = ((char *) buffer->unpack_ptr) - ((char *) buffer->base_ptr);
        buffer->base_ptr = (char *) realloc(buffer->base_ptr, to_alloc);
        memset(buffer->base_ptr + pack_offset, 0, to_alloc - buffer->bytes_allocated);
    } else {
        pack_offset = 0;
        unpack_offset = 0;
        buffer->bytes_used = 0;
        buffer->base_ptr = (char *) malloc(to_alloc);
        memset(buffer->base_ptr, 0, to_alloc);
    }

    if (NULL == buffer->base_ptr) {
        return NULL;
    }
    buffer->pack_ptr = ((char *) buffer->base_ptr) + pack_offset;
    buffer->unpack_ptr = ((char *) buffer->base_ptr) + unpack_offset;
    buffer->bytes_allocated = to_alloc;

    /* All done */
    return buffer->pack_ptr;
}

/*
 * Internal function that checks to see if the specified number of bytes
 * remain in the buffer for unpacking
 */
bool pmix_bfrop_too_small(pmix_buffer_t *buffer, size_t bytes_reqd)
{
    size_t bytes_remaining_packed;

    if (buffer->pack_ptr < buffer->unpack_ptr) {
        return true;
    }

    bytes_remaining_packed = buffer->pack_ptr - buffer->unpack_ptr;

    if (bytes_remaining_packed < bytes_reqd) {
        /* don't error log this - it could be that someone is trying to
         * simply read until the buffer is empty
         */
        return true;
    }

    return false;
}

pmix_status_t pmix_bfrop_store_data_type(pmix_pointer_array_t *regtypes, pmix_buffer_t *buffer,
                                         pmix_data_type_t type)
{
    pmix_status_t ret;

    PMIX_BFROPS_PACK_TYPE(ret, buffer, &type, 1, PMIX_UINT16, regtypes);
    return ret;
}

pmix_status_t pmix_bfrop_get_data_type(pmix_pointer_array_t *regtypes, pmix_buffer_t *buffer,
                                       pmix_data_type_t *type)
{
    pmix_status_t ret;
    int32_t m = 1;

    PMIX_BFROPS_UNPACK_TYPE(ret, buffer, type, &m, PMIX_UINT16, regtypes);
    return ret;
}

const char *pmix_bfrops_base_data_type_string(pmix_pointer_array_t *regtypes, pmix_data_type_t type)
{
    pmix_bfrop_type_info_t *info;

    /* Lookup the object for this type and call it */
    if (NULL == (info = (pmix_bfrop_type_info_t *) pmix_pointer_array_get_item(regtypes, type))) {
        return NULL;
    }

    return info->odti_name;
}

PMIX_EXPORT void *PMIx_Info_list_start(void)
{
    pmix_list_t *p;

    p = PMIX_NEW(pmix_list_t);
    return p;
}

PMIX_EXPORT pmix_status_t PMIx_Info_list_add(void *ptr,
                                             const char *key,
                                             const void *value,
                                             pmix_data_type_t type)
{
    pmix_list_t *p = (pmix_list_t *) ptr;
    pmix_infolist_t *iptr;

    iptr = PMIX_NEW(pmix_infolist_t);
    if (NULL == iptr) {
        return PMIX_ERR_NOMEM;
    }
    PMIX_INFO_LOAD(&iptr->info, key, value, type);
    pmix_list_append(p, &iptr->super);
    return PMIX_SUCCESS;
}

PMIX_EXPORT pmix_status_t PMIx_Info_list_insert(void *ptr,
                                                pmix_info_t *info)
{
    pmix_list_t *p = (pmix_list_t *) ptr;
    pmix_infolist_t *iptr;

    iptr = PMIX_NEW(pmix_infolist_t);
    if (NULL == iptr) {
        return PMIX_ERR_NOMEM;
    }
    /* we want to preserve any pointers in the provided
     * info struct so the result points to the same
     * memory location */
    memcpy(&iptr->info, info, sizeof(pmix_info_t));
    // mark that this value should not be released
    PMIX_INFO_SET_PERSISTENT(&iptr->info);
    pmix_list_append(p, &iptr->super);
    return PMIX_SUCCESS;
}

PMIX_EXPORT pmix_status_t PMIx_Info_list_xfer(void *ptr, const pmix_info_t *info)
{
    pmix_list_t *p = (pmix_list_t *) ptr;
    pmix_infolist_t *iptr;

    iptr = PMIX_NEW(pmix_infolist_t);
    if (NULL == iptr) {
        return PMIX_ERR_NOMEM;
    }
    PMIx_Info_xfer(&iptr->info, info);
    pmix_list_append(p, &iptr->super);
    return PMIX_SUCCESS;
}

PMIX_EXPORT pmix_status_t PMIx_Info_list_convert(void *ptr, pmix_data_array_t *par)
{
    pmix_list_t *p = (pmix_list_t *) ptr;
    size_t n;
    pmix_infolist_t *iptr;
    pmix_info_t *array;

    if (NULL == par || NULL == ptr) {
        return PMIX_ERR_BAD_PARAM;
    }
    PMIX_DATA_ARRAY_INIT(par, PMIX_INFO);

    n = pmix_list_get_size(p);
    if (0 == n) {
        return PMIX_ERR_EMPTY;
    }
    PMIX_INFO_CREATE(par->array, n);
    if (NULL == par->array) {
        return PMIX_ERR_NOMEM;
    }
    par->type = PMIX_INFO;
    par->size = n;
    array = (pmix_info_t *) par->array;

    /* transfer the elements across */
    n = 0;
    PMIX_LIST_FOREACH (iptr, p, pmix_infolist_t) {
        PMIx_Info_xfer(&array[n], &iptr->info);
        ++n;
    }

    return PMIX_SUCCESS;
}

PMIX_EXPORT void PMIx_Info_list_release(void *ptr)
{
    pmix_list_t *p = (pmix_list_t *) ptr;
    PMIX_LIST_RELEASE(p);
}

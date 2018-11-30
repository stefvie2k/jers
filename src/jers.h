/* Copyright (c) 2018 Evan Wyatt
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation 
 *    and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its contributors may
 *    be used to endorse or promote products derived from this software without
 *    specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) 
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */
#ifndef __JERS_H
#define __JERS_H

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "jers_error.h"

typedef uint32_t jobid_t;

#define JERS_VERSION "0.01"

#define JERS_RES_NAME_MAX 64
#define JERS_TAG_MAX 64

#define JERS_JOB_NAME_MAX 64
#define JERS_JOB_DEFAULT_PRIORITY 100

#define JERS_QUEUE_NAME_MAX 16
#define JERS_QUEUE_DESC_MAX 128
#define JERS_QUEUE_MAX_PRIORITY 128
#define JERS_QUEUE_MAX_LIMIT 1024
#define JERS_QUEUE_INVALID_CHARS "/\\ $"

/* Queue states are flags */
#define JERS_QUEUE_FLAG_STARTED 0x0001  // Jobs can run
#define JERS_QUEUE_FLAG_OPEN    0x0002  // Jobs can be submitted to this queue

#define JERS_QUEUE_DEFAULT_PRIORITY 100
#define JERS_QUEUE_DEFAULT_LIMIT 1
#define JERS_QUEUE_DEFAULT_STATE JERS_QUEUE_FLAG_OPEN // Open only

/* Job states are flags, so that filtering can applied with bitwise operations */

#define JERS_JOB_RUNNING   0x01
#define JERS_JOB_PENDING   0x02
#define JERS_JOB_DEFERRED  0x04
#define JERS_JOB_HOLDING   0x08
#define JERS_JOB_COMPLETED 0x10
#define JERS_JOB_EXITED    0x20

/* Field filter flags */
#define JERS_FILTER_JOBNAME   0x01
#define JERS_FILTER_QUEUE     0x02
#define JERS_FILTER_STATE     0x04
#define JERS_FILTER_TAGS      0x08
#define JERS_FILTER_RESOURCES 0x10
#define JERS_FILTER_UID       0x20

#define JERS_RET_JOBID   0x01
#define JERS_RET_QUEUE   0x02
#define JERS_RET_NAME    0x04
#define JERS_RET_STATE   0x08

#define JERS_QUEUE_

typedef struct jersJobAdd {
	char * name;
	char * queue;

	uid_t uid;

	char * shell;

	char * pre_cmd;
	char * post_cmd;

	char * stdout;
	char * stderr;

	time_t defer_time;

	int nice;
	int priority;
	char hold;

	int64_t argc;
	char ** argv;

	int64_t env_count;
	char ** envs;

	int64_t tag_count;
	char ** tags;

	int64_t res_count;
	char ** resources;
} jersJobAdd;

typedef struct jersJobMod {
	char * queue;

	time_t defer_time;

	int nice;
	int priority;
	char hold;

	int64_t env_count;
	char ** envs;

	int64_t tag_count;
	char ** tags;

	int64_t res_count;
	char ** resources;
} jersJobMod;

typedef struct jersJobDel {
	jobid_t jobid;
} jersJobDel;

typedef struct jersJob {
	jobid_t jobid;
	char * jobname;
	char * queue;
	int priority;
	int state;
	int nice;
	uid_t uid;

	char * shell;
	char * pre_command;
	char * post_command;

	char * stdout;
	char * stderr;

	int argc;
	char ** argv;

	time_t submit_time;
	time_t defer_time;
	time_t start_time;
	time_t finish_time;

	int tag_count;
	char ** tags;

	int res_count;
	char ** resources;

} jersJob;

typedef struct jersJobInfo {
	int64_t count;
	jersJob * jobs;
} jersJobInfo;

typedef struct jersJobFilter {
	int64_t filter_fields; // Bitmask of fields populated in filters. 0 == no filters
	int64_t return_fields; // Bitmask of fields to get returned; 0 == all fields
	
	jobid_t jobid;

	struct {
		char * job_name;
		char * queue_name;
		int state;
		char * tag;
		char * resource;
		uid_t uid;
	} filters;
} jersJobFilter;

typedef struct jersResource {
	char * name;
	int count;
	int inuse;
} jersResource;

typedef struct jersResourceInfo {
	int64_t count;
	jersResource * resources;
} jersResourceInfo;

typedef struct jersResourceAdd {
	char * name;
	int count;
} jersResourceAdd;

typedef struct jersResourceMod {
	char * name;
	int count;
} jersResourceMod;

typedef struct jersResourceFilter {
	int64_t filter_fields; 

	struct {
		char * name;
		int count;
	} filters;	
} jersResourceFilter;

struct job_stats {
	int64_t running;
	int64_t pending;
	int64_t holding;
	int64_t deferred;
	int64_t completed;
	int64_t exited;
};

typedef struct jersQueue {
	char * name;
	char * desc;
	char * node;
	int job_limit;
	int state;
	int priority;

	struct job_stats stats;
} jersQueue;

typedef struct jersQueueInfo {
	int64_t count;
	jersQueue * queues;
} jersQueueInfo;

typedef struct jersQueueAdd {
	char * name;
	char * node;
	char * desc;
	int state;
	int job_limit;
	int priority;
} jersQueueAdd;

typedef struct jersQueueMod {
	char * name;
	char * node;
	char * desc;
	int state;
	int job_limit;
	int priority;
} jersQueueMod;

typedef struct jersQueueFilter {
	int64_t filter_fields;

	struct {
		char *name;
	} filters;
} jersQueueFilter;

typedef struct jersQueueDel {
	char * name;
} jersQueueDel;

void jersInitJobAdd(jersJobAdd *j);
void jersInitJobMod(jersJobMod *j);

void jersInitQueueAdd(jersQueueAdd *q);
void jersInitQueueMod(jersQueueMod *q);

void jersInitResourceAdd(jersResourceAdd *r);
void jersInitResourceMod(jersResourceMod *r);

jobid_t jersAddJob(jersJobAdd *s);
int jersModJob(jobid_t id, jersJobMod *j);
int jersGetJob(jobid_t id, jersJobFilter *filter, jersJobInfo *info);
int jersDelJob(jobid_t id);
void jersFreeJobInfo (jersJobInfo *info);

int jersAddQueue(jersQueueAdd *q);
int jersModQueue(jersQueueMod *q);
int jersGetQueue(char *name, jersQueueFilter *filter, jersQueueInfo *info);
int jersDelQueue(jersQueueDel *q);
void jersFreeQueueInfo(jersQueueInfo *info);

int jersAddResource(char *name, int count);
int jersModResource(char *name, int new_count);
int jersGetResource(char *name, jersResourceFilter *filter, jersResourceInfo *info);
int jersDelResource(char *name);
void jersFreeResourceInfo(jersResourceInfo *info);

void jersFinish(void);

#endif
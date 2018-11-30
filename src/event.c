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
#include <server.h>
#include <unistd.h>

struct event * eventList = NULL;

void registerEvent(void(*func)(void), int interval) {
	struct event * e = calloc(sizeof(struct event), 1);

	e->func = func;
	e->interval = interval;

	/* Add it to the linked list of events */
	e->next = eventList;
	eventList = e;
	return;
}

void checkClientEvent(void) {
	client * c = server.client_list;

	/* Check the connected clients for commands to action,
	 * we can limit the amount of time we spend running command here.
	 * If we action a command and the client has more data, reinitalise
	 * the reader structure to run the next command */

	while (c) {
		if (c->msg.command)
			runCommand(c);

		c = c->next;
	}
}

void checkAgentEvent(void) {
	agent * a = server.agent_list;

	while (a) {
		while (a->msg.command)
			runAgentCommand(a);

		a = a->next;
	}
}

void checkDeferEvent(void) {
	releaseDeferred();
}

void flushEvent(void) {
	int64_t start = getTimeMS();
	int64_t end;

	if (!server.flush.dirty)
		return;

	fdatasync(server.state_fd);
	server.flush.dirty = 0;
	end = getTimeMS();
	print_msg(JERS_LOG_DEBUG, "state sync event took: %ldms\n", end - start);
}

void checkJobsEvent(void) {
	checkJobs();
}

void cleanupJobsEvent(void) {
	cleanupJobs(server.max_cleanup);
}

void backgroundSaveEvent(void) {
	stateSaveToDisk();
}

void initEvents(void) {
	registerEvent(checkJobsEvent, server.sched_freq);
	registerEvent(cleanupJobsEvent, 5000);
	registerEvent(backgroundSaveEvent, server.background_save_ms);

	if (server.flush.defer)
		registerEvent(flushEvent, server.flush.defer_ms);

	registerEvent(checkDeferEvent, 750);

	registerEvent(checkAgentEvent, 0);
	registerEvent(checkClientEvent, 0);
}

/* Check for any timed events to expire */

void checkEvents(void) {
	struct event * e = eventList;

	int64_t now = getTimeMS();

	while (e) {
		if (now >= e->last_fire + e->interval) {
			e->func();
			e->last_fire = getTimeMS();
		}

		e = e->next;
	}
}
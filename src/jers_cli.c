#include <stdlib.h>
#include <sys/types.h>
#include <pwd.h>
#include <signal.h>

#include <argp.h>
#include <jers_cli.h>
#include <common.h>

/* Options structures for each command */

/* Add Job */
static char add_job_doc[] = "add job -- Submit a new job into the scheduler";
static char add_job_arg_doc[] = "JOB_CMD -- ARG1 ...";
static struct argp_option add_job_options[] = {
	{"verbose", 'v', 0, 0, "Produce verbose output"},
	{"name", 'n', "job_name", 0, "Name of job"},
	{"stdout", 'o', "stdout_file", 0, "File to redirect job stdout to. Default /dev/null"},
	{"stderr", 'e', "stderr_file", 0, "File to redirect job stderr to. Default = stdout"},
	{"queue", 'q', "queue_name", 0, "Queue to submit job on"},
	{"hold", 'h', 0, 0, "Submit job on hold"},
	{"nice", 'N', "nice", 0, "Nice level of job"},
	{"priority", 'p', "priorty", 0, "Priority of job"},
	{"defer", 'd', "defer_time", 0, "Defer job until at least this time. Expected formats: '[yyyy-mm-dd] HH:MM[:SS]' or '@seconds_since_epoch'"},
	{"tag", 't', "key=value", 0, "Tag to add to job. Example 'type=type1' - Can be specified multiple times"},
	{"resource", 'r', "resource:count", 0, "Resource needed for job. Example 'software_licence:1' - Can be specified multiple times"},
	{"user", 'u', "username", 0, "User to submit job under"},
	{0}};

static int getUID(const char *username, uid_t *uid) {
	struct passwd * pw = getpwnam(username);

	if (pw == NULL)
		return 1;

	*uid = pw->pw_uid;

	return 0;
}

/* Translate a [yyyy-mm-dd] HH:MM[:SS] or @seconds time */
static time_t getDeferTime(const char *_time) {
	time_t t = 0;
	struct tm tm = {0};
	time_t now = time(NULL);
	

	if (_time[0] == '@')
		return atol(&_time[1]);

	// Load up the current time
	localtime_r(&now, &tm);
	tm.tm_isdst = -1;

	const char *space = strchr(_time, ' ');

	if (space) {
		/* Should have yyyy-mm-dd */
		int year, month, day;

		if (sscanf(_time, "%d-%d-%d", &year, &month, &day) != 3)
			return -1;
		
		tm.tm_year = year - 1900;
		tm.tm_mon = month - 1;
		tm.tm_mday = day;
		space++;
	} else {
		space = _time;
	}

	int hour,minutes,seconds = 0;
	if (sscanf(space, "%d:%d:%d", &hour, &minutes, &seconds) < 2)
		return -1;

	tm.tm_hour = hour;
	tm.tm_min = minutes;
	tm.tm_sec = seconds;

	t = mktime(&tm);

	return t;
}

static int getSignal(const char *str) {
	if (!isdigit(*str))
		return getSignalNumber(str);

	return atoi(str);
}

static int addResource(char *** res, int64_t * count, char *resource) {
	/* Expand the array to hold the resources */
	*res = realloc(*res, sizeof(char *) * (*count + 1));
 	*res[*count] = resource;
	(*count)++;
	return 0;
}

static int addTag(jers_tag_t **tags, int64_t *count, char *tag) {
	char *key = tag;
	char *value = strchr(tag, '=');

	if (value) {
		*value = '\0';
		value++;
	}

	*tags = realloc(*tags, sizeof(jers_tag_t) * (*count + 1));
	(*tags[*count]).key = key;
	(*tags[*count]).value = value;
	(*count)++;
	return 0;
}

static error_t add_job_parse(int key, char *arg, struct argp_state *state)
{
	struct add_job_args *arguments = state->input;

	switch (key)
	{
		case 'v':
			arguments->verbose = 1;
			break;

		case 'o':
			arguments->add.stdout = arg;
			break;

		case 'e':
			arguments->add.stderr = arg;
			break;

		case 'q':
			arguments->add.queue = arg;
			break;

		case 'n':
			arguments->add.name = arg;
			break;

		case 'N':
			arguments->add.nice = atoi(arg);
			break;

		case 'h':
			arguments->add.hold = 1;
			break;

		case 'd':
			if ((arguments->add.defer_time = getDeferTime(arg)) < 0) {
				fprintf(stderr, "Invalid defer time specified. Expecting '[yyyy-mm-hh] HH:MM[:SS]' or '@second_since_epoch'\n");
				return EINVAL;
			}

			break;

		case 'p':
			arguments->add.priority = atoi(arg);
			break;

		case 'u':
			if (getUID(arg, &arguments->add.uid) != 0) {
				fprintf(stderr, "Unable to determine uid of user %s\n", arg);
				return EINVAL;
			}
			break;

		case 'r':
			if (addResource(&arguments->add.resources, &arguments->add.res_count, arg) != 0) {
				fprintf(stderr, "Unable to add resource '%s' to job.\n", arg);
				return EINVAL;
			}
			break;

		case 't':
			if (addTag(&arguments->add.tags, &arguments->add.tag_count, arg) != 0) {
				fprintf(stderr, "Failed to add tag '%s' to job.", arg);
				return EINVAL;
			}
			break;

		case ARGP_KEY_INIT:
			jersInitJobAdd(&arguments->add);
			break;

		case ARGP_KEY_ARG:
			arguments->add.argc = state->argc - state->next + 1;
			arguments->add.argv = &state->argv[state->next - 1];

			/* Force parsing to stop */
			state->next = state->argc;
			break;

		case ARGP_KEY_END:
			break;

		default:
			return ARGP_ERR_UNKNOWN;
	}
	return 0;
}

static struct argp add_job_argp = {add_job_options, add_job_parse, add_job_arg_doc, add_job_doc};

/* Mod Job */
static char mod_job_doc[] = "mod job - Modify existing job/s";
static char mod_job_arg_doc[] = "jobid [...]";
static struct argp_option mod_job_options[] = {
	{"verbose", 'v', 0, 0, "Produce verbose output"},
	{"name", 'n', "job_name", 0, "Name of job"},
	{"queue", 'q', "queue_name", 0, "Queue to submit job on"},
	{"hold", 'h', 0, 0, "Submit job on hold"},
	{"priority", 'p', "priority", 0, "Priority of job"},
	{"nice", 'N', "nice", 0, "Nice level of job"},
	{"defer", 'd', "defer_time", 0, "Defer job until at least this time. Expected formats: '[yyyy-mm-dd] HH:MM[:SS]' or '@seconds_since_epoch'"},
	{"tag", 't', "key=value", 0, "Tag to add to job. Example 'type=type1' - Can be specified multiple times"},
	{"resource", 'r', "resource:count", 0, "Resource needed for job. Example 'software_licence:1' - Can be specified multiple times"},
	{"restart", 'R', 0, 0, "Restart specified job/s"},
	{0}
};


static error_t mod_job_parse(int key, char *arg, struct argp_state *state)
{
	struct mod_job_args *arguments = state->input;
	int count = 0;

	switch (key)
	{
		case 'v':
			arguments->verbose = 1;
			break;

		case 'q':
			arguments->mod.queue = arg;
			break;

		case 'n':
			arguments->mod.name = arg;
			break;

		case 'N':
			arguments->mod.nice = atoi(arg);
			break;

		case 'h':
			arguments->mod.hold = 1;
			break;

		case 'R':
			arguments->mod.restart = 1;
			break;

		case 'd':
			if ((arguments->mod.defer_time = getDeferTime(arg)) < 0) {
				fprintf(stderr, "Invalid defer time specified. Expecting '[yyyy-mm-hh] HH:MM[:SS]' or '@second_since_epoch'\n");
				return EINVAL;
			}

			break;

		case 'p':
			arguments->mod.priority = atoi(arg);
			break;

		case 'r':
			if (addResource(&arguments->mod.resources, &arguments->mod.res_count, arg) != 0) {
				fprintf(stderr, "Unable to add resource '%s' to job.\n", arg);
				return EINVAL;
			}
			break;

		case 't':
			if (addTag(&arguments->mod.tags, &arguments->mod.tag_count, arg) != 0) {
				fprintf(stderr, "Failed to add tag '%s' to job.", arg);
				return EINVAL;
			}
			break;

		case ARGP_KEY_INIT:
			jersInitJobMod(&arguments->mod);
			break;

		case ARGP_KEY_ARG:
			count = state->argc - state->next + 1;
			arguments->jobids = calloc(count + 1, sizeof(jobid_t));
			for (int i = 0; i < count; i++)
				arguments->jobids[i] = atoi(state->argv[state->next - 1 + i]);

			/* Force parsing to stop */
			state->next = state->argc;

			break;

		case ARGP_KEY_END:
			break;

		default:
			return ARGP_ERR_UNKNOWN;
	}
	return 0;
}

static struct argp mod_job_argp = {mod_job_options, mod_job_parse, mod_job_arg_doc, mod_job_doc};



/* Mod Queue */
static char mod_queue_doc[] = "mod queue - Modify existing queue/s";
static char mod_queue_arg_doc[] = "QUEUENAME [...]";
static struct argp_option mod_queue_options[] = {
	{"verbose", 'v', 0, 0, "Produce verbose output"},
	{"description", 'd', "description", 0, "Queue description"},
	{"host", 'h', "host", 0, "Host queue runs on"},
	{"limit", 'l', "job_limit", 0, "Queue job limit"},
	{"priority", 'p', "priority", 0, "Queue priority"},
	{"state", 's', "state", 0, "Queue state. <open/closed>:<started/stopped>"},
	{0}
};


//TODO: this needs work. Would like to be able to specify only part of the state
//      That would require knowing the queue state up front.
static int getState(char *state) {
	/* States look like <open/closed>:<started/stopped> */
	char *ptr = strchr(state, ':');
	int s = 0;

	if (ptr == NULL)
		return -1;

	*ptr = '\0';
	ptr++;

	/* First section */
	if (strcasecmp(state, "OPEN") == 0)
		s = JERS_QUEUE_FLAG_OPEN;
	else if (strcasecmp(state, "CLOSED") == 0)
		s = 0;
	else
		return -1;

	/* Second section */
	if (strcasecmp(ptr, "STARTED") == 0)
		s |= JERS_QUEUE_FLAG_STARTED;
	else if (strcasecmp(ptr, "STOPPED") == 0)
		(void)s;
	else
		return -1;

	return s;
}

static error_t mod_queue_parse(int key, char *arg, struct argp_state *state)
{
	struct mod_queue_args *arguments = state->input;
	int count = 0;

	switch (key)
	{
		case 'v':
			arguments->verbose = 1;
			break;

		case 'd':
			arguments->mod.desc = arg;
			break;

		case 'h':
			arguments->mod.node = arg;
			break;

		case 'l':
			arguments->mod.job_limit = atoi(arg);
			break;

		case 'p':
			arguments->mod.priority = atoi(arg);
			break;

		case 's':
			arguments->mod.state = getState(arg);

			if (arguments->mod.state < 0) {
				fprintf(stderr, "Invalid state specified\n");
				return EINVAL;
			}

			break;

		case ARGP_KEY_INIT:
			jersInitQueueMod(&arguments->mod);
			break;

		case ARGP_KEY_ARG:
			count = state->argc - state->next + 1;
			arguments->queues = calloc(count + 1, sizeof(char *));
			for (int i = 0; i < count; i++)
				arguments->queues[i] = state->argv[state->next - 1 + i];

			/* Force parsing to stop */
			state->next = state->argc;
			break;

		case ARGP_KEY_END:
			break;

		default:
			return ARGP_ERR_UNKNOWN;
	}
	return 0;
}

static struct argp mod_queue_argp = {mod_queue_options, mod_queue_parse, mod_queue_arg_doc, mod_queue_doc};



/* Add Queue */
static char add_queue_doc[] = "add queue - Create a new queue";
static char add_queue_arg_doc[] = "QUEUENAME";
static struct argp_option add_queue_options[] = {
	{"verbose", 'v', 0, 0, "Produce verbose output"},
	{"description", 'd', "description", 0, "Queue description"},
	{"host", 'h', "host", 0, "Host queue runs on"},
	{"limit", 'l', "job_limit", 0, "Queue job limit"},
	{"priority", 'p', "priority", 0, "Queue priority"},
	{0}
};

static error_t add_queue_parse(int key, char *arg, struct argp_state *state)
{
	struct add_queue_args *arguments = state->input;

	switch (key)
	{
		case 'v':
			arguments->verbose = 1;
			break;

		case 'd':
			arguments->add.desc = arg;
			break;

		case 'h':
			arguments->add.node = arg;
			break;

		case 'l':
			arguments->add.job_limit = atoi(arg);
			break;

		case 'p':
			arguments->add.priority = atoi(arg);
			break;

		case ARGP_KEY_INIT:
			jersInitQueueAdd(&arguments->add);
			break;

		case ARGP_KEY_ARG:
			arguments->add.name = arg;
			break;

		case ARGP_KEY_END:
			break;

		default:
			return ARGP_ERR_UNKNOWN;
	}
	return 0;
}

static struct argp add_queue_argp = {add_queue_options, add_queue_parse, add_queue_arg_doc, add_queue_doc};

/* Add Job */
static char show_job_doc[] = "show job -- Show jobs";
static char show_job_arg_doc[] = "JOBID";
static struct argp_option show_job_options[] = {
	{"verbose", 'v', 0, 0, "Produce verbose output"},
	{"all", 'a', 0, 0, "Display all details of the job/s"},
	{0}};

static error_t show_job_parse(int key, char *arg, struct argp_state *state)
{
	UNUSED(arg);
	int count = 0;
	struct show_job_args *arguments = state->input;

	switch (key)
	{
		case 'v':
			arguments->verbose = 1;
			break;

		case 'a':
			arguments->all = 1;
			break;

		case ARGP_KEY_INIT:
			break;

		case ARGP_KEY_ARG:
			count = state->argc - state->next + 1;
			arguments->jobids = calloc(count + 1, sizeof(jobid_t));
			for (int i = 0; i < count; i++)
				arguments->jobids[i] = atoi(state->argv[state->next - 1 + i]);

			/* Force parsing to stop */
			state->next = state->argc;

			break;

		case ARGP_KEY_END:
			break;

		default:
			return ARGP_ERR_UNKNOWN;
	}
	return 0;
}

static struct argp show_job_argp = {show_job_options, show_job_parse, show_job_arg_doc, show_job_doc};

static char show_queue_doc[] = "show queue -- Show queues";
static char show_queue_arg_doc[] = "QUEUE";
static struct argp_option show_queue_options[] = {
	{"verbose", 'v', 0, 0, "Produce verbose output"},
	{"all", 'a', 0, 0, "Display all details of the queue/s"},
	{0}};

static error_t show_queue_parse(int key, char *arg, struct argp_state *state)
{
	UNUSED(arg);
	int count = 0;
	struct show_queue_args *arguments = state->input;

	switch (key)
	{
		case 'v':
			arguments->verbose = 1;
			break;

		case 'a':
			arguments->all = 1;
			break;

		case ARGP_KEY_INIT:
			break;

		case ARGP_KEY_ARG:
			count = state->argc - state->next + 1;
			arguments->queues = calloc(count + 1, sizeof(char *));
			for (int i = 0; i < count; i++)
				arguments->queues[i] = state->argv[state->next - 1 + i];

			/* Force parsing to stop */
			state->next = state->argc;
			break;

		case ARGP_KEY_END:
			break;

		default:
			return ARGP_ERR_UNKNOWN;
	}
	return 0;
}

static struct argp show_queue_argp = {show_queue_options, show_queue_parse, show_queue_arg_doc, show_queue_doc};


static char delete_queue_doc[] = "delete queue -- Delete queue/s";
static char delete_queue_arg_doc[] = "QUEUE";
static struct argp_option delete_queue_options[] = {
	{"verbose", 'v', 0, 0, "Produce verbose output"},
	{0}};

static error_t delete_queue_parse(int key, char *arg, struct argp_state *state)
{
	UNUSED(arg);
	int count = 0;
	struct delete_queue_args *arguments = state->input;

	switch (key)
	{
		case 'v':
			arguments->verbose = 1;
			break;

		case ARGP_KEY_INIT:
			break;

		case ARGP_KEY_ARG:
			count = state->argc - state->next + 1;
			arguments->queues = calloc(count + 1, sizeof(char *));
			for (int i = 0; i < count; i++)
				arguments->queues[i] = state->argv[state->next - 1 + i];

			/* Force parsing to stop */
			state->next = state->argc;
			break;

		case ARGP_KEY_END:
			break;

		default:
			return ARGP_ERR_UNKNOWN;
	}
	return 0;
}

static struct argp delete_queue_argp = {delete_queue_options, delete_queue_parse, delete_queue_arg_doc, delete_queue_doc};

static char delete_job_doc[] = "delete job -- Delete job/s";
static char delete_job_arg_doc[] = "JOBID";
static struct argp_option delete_job_options[] = {
	{"verbose", 'v', 0, 0, "Produce verbose output"},
	{0}};

static error_t delete_job_parse(int key, char *arg, struct argp_state *state)
{
	UNUSED(arg);
	int count = 0;
	struct delete_job_args *arguments = state->input;

	switch (key)
	{
		case 'v':
			arguments->verbose = 1;
			break;

		case ARGP_KEY_INIT:
			break;

		case ARGP_KEY_ARG:
			count = state->argc - state->next + 1;
			arguments->jobids = calloc(count + 1, sizeof(jobid_t));
			for (int i = 0; i < count; i++)
				arguments->jobids[i] = atoi(state->argv[state->next - 1 + i]);

			/* Force parsing to stop */
			state->next = state->argc;

			break;


		case ARGP_KEY_END:
			break;

		default:
			return ARGP_ERR_UNKNOWN;
	}
	return 0;
}

static struct argp delete_job_argp = {delete_job_options, delete_job_parse, delete_job_arg_doc, delete_job_doc};

static char signal_job_doc[] = "signal job -- Signal job/s";
static char signal_job_arg_doc[] = "JOBID";
static struct argp_option signal_job_options[] = {
	{"verbose", 'v', 0, 0, "Produce verbose output"},
	{"signal" , 's', "SIGNAL", 0, "Signal to send. May be give as a signal name or number. Defaults to SIGTERM"},
	{0}};

static error_t signal_job_parse(int key, char *arg, struct argp_state *state)
{
	int count = 0;
	struct signal_job_args *arguments = state->input;

	switch (key)
	{
		case 'v':
			arguments->verbose = 1;
			break;

		case 's':
			arguments->signal = getSignal(arg);
			if (arguments->signal < 0 || arguments->signal >= NSIG) {
				fprintf(stderr, "Invalid signal specified '%s'\n", arg);
				return EINVAL;
			}

			break;

		case ARGP_KEY_INIT:
			arguments->signal = SIGTERM;
			break;

		case ARGP_KEY_ARG:
			count = state->argc - state->next + 1;
			arguments->jobids = calloc(count + 1, sizeof(jobid_t));
			for (int i = 0; i < count; i++)
				arguments->jobids[i] = atoi(state->argv[state->next - 1 + i]);

			/* Force parsing to stop */
			state->next = state->argc;

			break;


		case ARGP_KEY_END:
			break;

		default:
			return ARGP_ERR_UNKNOWN;
	}
	return 0;
}

static struct argp signal_job_argp = {signal_job_options, signal_job_parse, signal_job_arg_doc, signal_job_doc};


static char add_resource_doc[] = "add resource -- Add a new resource";
static char add_resource_arg_doc[] = "resource_name[:count] [...]";
static struct argp_option add_resource_options[] = {
	{"verbose", 'v', 0, 0, "Produce verbose output"},
	{0}
};

static error_t add_resource_parse(int key, char *arg, struct argp_state *state)
{
	UNUSED(arg);
	int count = 0;
	struct add_resource_args *arguments = state->input;

	switch (key)
	{
		case 'v':
			arguments->verbose = 1;
			break;

		case ARGP_KEY_INIT:
			break;

		case ARGP_KEY_ARG:
			count = state->argc - state->next + 1;
			arguments->resources = calloc(count + 1, sizeof(char *));
			for (int i = 0; i < count; i++)
				arguments->resources[i] = state->argv[state->next - 1 + i];

			/* Force parsing to stop */
			state->next = state->argc;

			break;


		case ARGP_KEY_END:
			break;

		default:
			return ARGP_ERR_UNKNOWN;
	}
	return 0;
}

static struct argp add_resource_argp = {add_resource_options, add_resource_parse, add_resource_arg_doc, add_resource_doc};


static char mod_resource_doc[] = "mod resource -- Mod a existing resource/s";
static char mod_resource_arg_doc[] = "resource_name:new_count [...]";
static struct argp_option mod_resource_options[] = {
	{"verbose", 'v', 0, 0, "Produce verbose output"},
	{0}
};

static error_t mod_resource_parse(int key, char *arg, struct argp_state *state)
{
	UNUSED(arg);
	int count = 0;
	struct mod_resource_args *arguments = state->input;

	switch (key)
	{
		case 'v':
			arguments->verbose = 1;
			break;

		case ARGP_KEY_INIT:
			break;

		case ARGP_KEY_ARG:
			count = state->argc - state->next + 1;
			arguments->resources = calloc(count + 1, sizeof(char *));
			for (int i = 0; i < count; i++)
				arguments->resources[i] = state->argv[state->next - 1 + i];

			/* Force parsing to stop */
			state->next = state->argc;

			break;


		case ARGP_KEY_END:
			break;

		default:
			return ARGP_ERR_UNKNOWN;
	}
	return 0;
}

static struct argp mod_resource_argp = {mod_resource_options, mod_resource_parse, mod_resource_arg_doc, mod_resource_doc};


static char del_resource_doc[] = "del resource -- Delete existing resource/s";
static char del_resource_arg_doc[] = "resource_name [...]";
static struct argp_option del_resource_options[] = {
	{"verbose", 'v', 0, 0, "Produce verbose output"},
	{0}
};

static error_t del_resource_parse(int key, char *arg, struct argp_state *state)
{
	UNUSED(arg);
	int count = 0;
	struct del_resource_args *arguments = state->input;

	switch (key)
	{
		case 'v':
			arguments->verbose = 1;
			break;

		case ARGP_KEY_INIT:
			break;

		case ARGP_KEY_ARG:
			count = state->argc - state->next + 1;
			arguments->resources = calloc(count + 1, sizeof(char *));
			for (int i = 0; i < count; i++)
				arguments->resources[i] = state->argv[state->next - 1 + i];

			/* Force parsing to stop */
			state->next = state->argc;

			break;


		case ARGP_KEY_END:
			break;

		default:
			return ARGP_ERR_UNKNOWN;
	}
	return 0;
}

static struct argp del_resource_argp = {del_resource_options, del_resource_parse, del_resource_arg_doc, del_resource_doc};

static char show_resource_doc[] = "show resource -- Show resource/s";
static char show_resource_arg_doc[] = "resource_name [...]";
static struct argp_option show_resource_options[] = {
	{"verbose", 'v', 0, 0, "Produce verbose output"},
	{0}
};

static error_t show_resource_parse(int key, char *arg, struct argp_state *state)
{
	UNUSED(arg);
	int count = 0;
	struct show_resource_args *arguments = state->input;

	switch (key)
	{
		case 'v':
			arguments->verbose = 1;
			break;

		case ARGP_KEY_INIT:
			break;

		case ARGP_KEY_ARG:
			count = state->argc - state->next + 1;
			arguments->resources = calloc(count + 1, sizeof(char *));
			for (int i = 0; i < count; i++)
				arguments->resources[i] = state->argv[state->next - 1 + i];

			/* Force parsing to stop */
			state->next = state->argc;

			break;

		case ARGP_KEY_END:
			break;

		default:
			return ARGP_ERR_UNKNOWN;
	}
	return 0;
}

static struct argp show_resource_argp = {show_resource_options, show_resource_parse, show_resource_arg_doc, show_resource_doc};


int parse_args(int argc, char *argv[], struct argp *argp, void *args) {
	/* Call the argp parser, skipping the first two arguments of command/object */
	if (argp_parse (argp, argc - 2, &argv[2], 0, 0, args) != 0) {
		fprintf(stderr, "Failed to parse arguments\n");
		return 1;
	}

	return 0;
}

int parse_add_job(int argc, char * argv[], struct add_job_args * args) {
	memset(args, 0, sizeof(struct add_job_args));
	return parse_args(argc, argv, &add_job_argp, args);
}

int parse_add_queue(int argc, char * argv[], struct add_queue_args * args) {
	memset(args, 0, sizeof(struct add_queue_args));
	return parse_args(argc, argv, &add_queue_argp, args);
}

int parse_show_job(int argc, char * argv[], struct show_job_args * args) {
	memset(args, 0, sizeof(struct show_job_args));
	return parse_args(argc, argv, &show_job_argp, args);
}

int parse_show_queue(int argc, char * argv[], struct show_queue_args * args) {
	memset(args, 0, sizeof(struct show_queue_args));
	return parse_args(argc, argv, &show_queue_argp, args);
}

int parse_delete_queue(int argc, char * argv[], struct delete_queue_args * args) {
	memset(args, 0, sizeof(struct delete_queue_args));
	return parse_args(argc, argv, &delete_queue_argp, args);
}

int parse_delete_job(int argc, char * argv[], struct delete_job_args * args) {
	memset(args, 0, sizeof(struct delete_job_args));
	return parse_args(argc, argv, &delete_job_argp, args);
}

int parse_signal_job(int argc, char * argv[], struct signal_job_args * args) {
	memset(args, 0, sizeof(struct signal_job_args));
	return parse_args(argc, argv, &signal_job_argp, args);
}

int parse_add_resource(int argc, char *argv[], struct add_resource_args *args) {
	memset(args, 0, sizeof(struct add_resource_args));
	return parse_args(argc, argv, &add_resource_argp, args);
}

int parse_mod_resource(int argc, char *argv[], struct mod_resource_args *args) {
	memset(args, 0, sizeof(struct mod_resource_args));
	return parse_args(argc, argv, &mod_resource_argp, args);
}

int parse_del_resource(int argc, char *argv[], struct del_resource_args *args) {
	memset(args, 0, sizeof(struct del_resource_args));
	return parse_args(argc, argv, &del_resource_argp, args);
}

int parse_show_resource(int argc, char *argv[], struct show_resource_args *args) {
	memset(args, 0, sizeof(struct show_resource_args));
	return parse_args(argc, argv, &show_resource_argp, args);
}

int parse_mod_queue(int argc, char *argv[], struct mod_queue_args *args) {
	memset(args, 0, sizeof(struct mod_queue_args));
	return parse_args(argc, argv, &mod_queue_argp, args);
}

int parse_mod_job(int argc, char *argv[], struct mod_job_args *args) {
	memset(args, 0, sizeof(struct mod_job_args));
	return parse_args(argc, argv, &mod_job_argp, args);
}
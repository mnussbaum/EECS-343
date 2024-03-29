/***************************************************************************
 *  Title: Runtime environment
 * -------------------------------------------------------------------------
 *    Purpose: Runs commands
 *    Author: Stefan Birrer
 *    Version: $Revision: 1.3 $
 *    Last Modification: $Date: 2009/10/12 20:50:12 $
 *    File: $RCSfile: runtime.c,v $
 *    Copyright: (C) 2002 by Stefan Birrer
 ***************************************************************************/
/***************************************************************************
 *  ChangeLog:
 * -------------------------------------------------------------------------
 *    $Log: runtime.c,v $
 *    Revision 1.3  2009/10/12 20:50:12  jot836
 *    Commented tsh C files
 *
 *    Revision 1.2  2009/10/11 04:45:50  npb853
 *    Changing the identation of the project to be GNU.
 *
 *    Revision 1.1  2005/10/13 05:24:59  sbirrer
 *    - added the skeleton files
 *
 *    Revision 1.6  2002/10/24 21:32:47  sempi
 *    final release
 *
 *    Revision 1.5  2002/10/23 21:54:27  sempi
 *    beta release
 *
 *    Revision 1.4  2002/10/21 04:49:35  sempi
 *    minor correction
 *
 *    Revision 1.3  2002/10/21 04:47:05  sempi
 *    Milestone 2 beta
 *
 *    Revision 1.2  2002/10/15 20:37:26  sempi
 *    Comments updated
 *
 *    Revision 1.1  2002/10/15 20:20:56  sempi
 *    Milestone 1
 *
 ***************************************************************************/
#define __RUNTIME_IMPL__

/************System include***********************************************/
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>

/************Private include**********************************************/
#include "runtime.h"
#include "interpreter.h"
#include "io.h"

/************Defines and Typedefs*****************************************/
/*  #defines and typedefs should have their names in all caps.
 *  Global variables begin with g. Global constants with k. Local
 *  variables should be in all lower case. When initializing
 *  structures and arrays, line everything up in neat columns.
 */

/************Global Variables*********************************************/

#define NBUILTINCOMMANDS (sizeof BuiltInCommands / sizeof(char*))

/* the pids of the background processes */
bgjobL *bgjobs = NULL;
bgjobL *oldest_bgjob = NULL;

const int JOB_RUNNING = 0;
const int JOB_STOPPED = 1;
const int JOB_DONE = 2;
/************Function Prototypes******************************************/

void
waitfg(void);

/* run command */
static void
RunCmdFork(commandT*, bool);
/* runs an external program command after some checks */
static void
RunExternalCmd(commandT*, bool);
/* resolves the path and checks for exutable flag */
static bool
ResolveExternalCmd(commandT*);
/* forks and runs a external program */
static void
Exec(commandT*, bool, bool, bool, int[2]);
/* runs a builtin command */
static void
RunBuiltInCmd(commandT*);
/* checks whether a command is a builtin command */
static bool
IsBuiltIn(char*);

void
freeCommandTList(commandT_list* cmd_cell);

bool
is_bg(commandT *cmd);
int
job_stack_size();

bgjobL*
pop_bg_job(pid_t);

bgjobL *
delete_job_num(int job_num);

bgjobL *
continue_job_num(int job_num);

int
size_of_bgjobs(void);

void
print_aliases(void);

void
make_alias(commandT*);

void
push_alias(aliasT*);

void
remove_alias(char*);

char*
is_alias_for(char*);

commandT*
resolve_aliases(commandT*);

aliasT *aliases;
/************External Declaration*****************************************/

/**************Implementation***********************************************/

/*
 * RunCmd
 *
 * arguments:
 *   commandT *cmd: the command to be run
 *
 * returns: none
 *
 * Runs the given command.
 */
	void
RunCmd(commandT* cmd)
{
	RunCmdFork(cmd, TRUE);
} /* RunCmd */


/*
 * RunCmdFork
 *
 * arguments:
 *   commandT *cmd: the command to be run
 *   bool fork: whether to fork
 *
 * returns: none
 *
 * Runs a command, switching between built-in and external mode
 * depending on cmd->argv[0].
 */
	void
RunCmdFork(commandT* cmd, bool fork)
{
	commandT* new_cmd;
        if(strcmp(cmd->name, "unalias") == 0) 
		new_cmd = cmd;
	else
		new_cmd = resolve_aliases(cmd);
    	
	//    freeCommand(cmd);
	if (new_cmd->argc <= 0)
		return;
        if (IsBuiltIn(new_cmd->argv[0]))
	{
		RunBuiltInCmd(new_cmd);
	}
	else
	{
		RunExternalCmd(new_cmd, fork);
	}
} /* RunCmdFork */

commandT*
resolve_aliases(commandT* cmd)
{
	//hardcoding 100 instead of exporting MAXARGS
    commandT* new_cmd = malloc(sizeof(commandT)+ sizeof(char *)* 100);
    int i = 0;
    int new_arg_count = 0;
    bool inputQuoted = FALSE;
    for(i=0; i<cmd->argc; i++)
    {
        char* cur_arg = cmd->argv[i];
        if(strchr(cur_arg, ' '))
		inputQuoted = TRUE;
	char* new = is_alias_for(cur_arg);
        char* tmp = malloc(sizeof(char) * MAXLINE);
        int tmp_len = 0;
        int j;
        for(j=0; j<=strlen(new); j++)
        {
            char cur_char = new[j];
            if(((cur_char == ' ' || cur_char == 0) && !inputQuoted) || (inputQuoted && cur_char == 0))
            {
                // a new argument is ended
                tmp[tmp_len] = 0;
                tmp_len = 0;
                char* new_arg = malloc(sizeof(char) *(tmp_len + 1));
              //  printf("new arg: %s\n", new_arg); garbage line?
                strcpy(new_arg, tmp);
                new_cmd->argv[new_arg_count] = new_arg;
          //      printf("new argv: %s\n", new_cmd->argv[new_arg_count]);
                new_arg_count++;
                tmp = realloc(tmp, sizeof(char) * MAXLINE);
            }         
	    else
            {
                tmp[tmp_len] = cur_char;
                tmp_len++;
            }
        }
	inputQuoted = FALSE;
        free(tmp);
        //free(cur_arg); freeCommand will handle this after this func returns
    }
    new_cmd->name = new_cmd->argv[0];
    new_cmd->argc = new_arg_count;
    new_cmd->argv[new_cmd->argc] = 0;
    freeCommand(cmd);
    return new_cmd;
}

char *
is_alias_for(char* arg)
{
    char* result = malloc(sizeof(char) * MAXLINE);
    strcpy(result, arg);
    aliasT* top_alias = aliases;
    while(top_alias)
    {
        if(strcmp(arg, top_alias->lhs) == 0)
        {
            strcpy(result, top_alias->rhs);
            break;
        }
        top_alias = top_alias->next;
    }
    return result;
}


/*
 * RunCmdBg
 *
 * arguments:
 *   commandT *cmd: the command to be run
 *
 * returns: none
 *
 * Runs a command in the background.
 */
	void
RunCmdBg(commandT* cmd)
{
	// TODO
} /* RunCmdBg */


/*
 * RunCmdPipe
 *
 * arguments:
 *   commandT *cmd1: the commandT struct for the left hand side of the pipe
 *   commandT *cmd2: the commandT struct for the right hand side of the pipe
 *
 * returns: none
 *
 * Runs two commands, redirecting standard output from the first to
 * standard input on the second.
 */
    void
RunCmdPipe(commandT_list* commands)
{
    int old_fd[2];
    commandT_list* top_cmd = commands;
    commandT_list* prev_cmd = NULL;
    while(top_cmd)
    {
        Exec(top_cmd->cmd, TRUE, (top_cmd->next != NULL),
             (prev_cmd != NULL), old_fd);
        if(prev_cmd)
            freeCommandTList(prev_cmd);
        prev_cmd = top_cmd;
        top_cmd = top_cmd->next;
    }
    if(prev_cmd)
        freeCommandTList(prev_cmd);
} /* RunCmdPipe */


void
freeCommandTList(commandT_list* cmd_cell)
{
    //freeCommand(cmd_cell->cmd);
    free(cmd_cell);
}

/*
 * RunCmdRedirOut
 *
 * arguments:
 *   commandT *cmd: the command to be run
 *   char *file: the file to be used for standard output
 *
 * returns: none
 *
 * Runs a command, redirecting standard output to a file.
 */
	void
RunCmdRedirOut(commandT* cmd, char* file)
{
} /* RunCmdRedirOut */


/*
 * RunCmdRedirIn
 *
 * arguments:
 *   commandT *cmd: the command to be run
 *   char *file: the file to be used for standard input
 *
 * returns: none
 *
 * Runs a command, redirecting a file to standard input.
 */
	void
RunCmdRedirIn(commandT* cmd, char* file)
{
}  /* RunCmdRedirIn */


/*
 * RunExternalCmd
 *
 * arguments:
 *   commandT *cmd: the command to be run
 *   bool fork: whether to fork
 *
 * returns: none
 *
 * Tries to run an external command.
 */
	static void
RunExternalCmd(commandT* cmd, bool fork)
{
	if (ResolveExternalCmd(cmd))
		Exec(cmd, fork, FALSE, FALSE, NULL);
	else if(strcmp(cmd->name, "exit")!=0) //if exit, let interpreter handle it
	{
		Print("/bin/bash: line 6: ");
		Print(cmd->name);
		Print(": ");
		Print("command not found\n");
                freeCommand(cmd);
	}
}  /* RunExternalCmd */


/*
 * ResolveExternalCmd
 ::*
 * arguments:
 *   commandT *cmd: the command to be run
 *
 * returns: bool: whether the given command exists
 *
 * Determines whether the command to be run actually exists.
 */
	static bool
ResolveExternalCmd(commandT* cmd)
{
	char *path;
	char *pathtoken;
	char attemptPath[256];
	if(FileExists(cmd->name)) //checks local and absolute path
		return TRUE;

	path = getenv("PATH");  //otherwise get path

	size_t const len = strlen(path)+1;
	char *copypath = memcpy(malloc(len), path, len); //copy it into new memory
	pathtoken = strtok(copypath,":"); //tokenize using : as delimiter

	while(pathtoken != NULL)
	{
		strncpy(attemptPath,pathtoken,63); //copy token into a tmp var
		strncat(attemptPath,"/",1); //add on a slash
		strncat(attemptPath,cmd->name,194); // and the command name
		if(FileExists(attemptPath)) //check for existence (absolute)
		{
			free(copypath); //if found, free and return
			return TRUE;
		}
		pathtoken = strtok(NULL, ":"); //otherwise continue to tokenize
	}
	free(copypath);
	return FALSE;
} /* ResolveExternalCmd */


/*
 * Exec
 *
 * arguments:
 *   commandT *cmd: the command to be run
 *   bool forceFork: whether to fork
 *
 * returns: none
 *
 * Executes a command.
 */
static void
Exec(commandT* cmd, bool forceFork, bool next, bool prev,
     int old_fd[2])
{
	int new_fd[2];
	pid_t pid;
	char *path;
	char *pathtoken;
	char attemptPath[256];
	bool onPath = FALSE;
        bool make_bg = FALSE;
        if(is_bg(cmd))
            make_bg = TRUE;

        if(next)
            pipe(new_fd);

	
	if(!FileExists(cmd->name)) //if you cant find it locally, we need to go find it on the path
	{
		path = getenv("PATH");
		size_t const len = strlen(path)+1;
		char *copypath = memcpy(malloc(len), path, len);
		pathtoken = strtok(copypath,":");
		while(pathtoken != NULL) // same as before, this time
		{
			strncpy(attemptPath,pathtoken, 63);
			strncat(attemptPath,"/",1);
			strncat(attemptPath,cmd->name,194);
			if(FileExists(attemptPath))
			{
				onPath = TRUE;
				break;
			}
			pathtoken = strtok(NULL, ":");
		}
		free(copypath);
	}
	if(!onPath) //wasnt found on path, must be local/absolute
	{
		memset(attemptPath, 0, 256);
		memcpy(attemptPath, cmd->name,256);
	}

	int i = 0;
	char *tmp;
	char *env;
	char *tildeloc;
	for (i = 0; i <cmd->argc; i++)
	{
		if((tildeloc =strchr(cmd->argv[i],'~')))
		{
		tildeloc[0] = 0;
		tmp=malloc(sizeof(char) *MAXLINE);
 		
		strcpy(tmp, cmd->argv[i]); 	
		env = getenv("HOME");
		strcat(tmp, env);
		strcat(tmp, tildeloc+1);
		free(cmd->argv[i]);
		cmd->argv[i] = tmp;
		}
		
	} 

	if(forceFork) //if told to fork...
	{
            sigset_t sigs;
            sigemptyset(&sigs);
            sigaddset(&sigs, SIGCHLD);
            sigprocmask(SIG_BLOCK, &sigs, NULL);

            pid = fork(); //fork
            if(pid == 0) //Child
            {
                sigprocmask(SIG_UNBLOCK, &sigs, NULL);
                setpgid(0, 0);
                if(prev)
                {
                    dup2(old_fd[0], 0);
                    close(old_fd[0]);
                    close(old_fd[1]);
                }
                if(next)
                {
                    close(new_fd[0]);
                    dup2(new_fd[1], 1);
                    close(new_fd[1]);
                }
                execv(attemptPath, cmd->argv);
            }
            if (make_bg)
            {
                push_bg_job(pid, cmd);
                sigprocmask(SIG_UNBLOCK, &sigs, NULL);
            }
            else
            {
                fg_pgid = pid;
                fg_cmd = cmd;
                sigprocmask(SIG_UNBLOCK, &sigs, NULL);
                if(prev)
                {
                    close(old_fd[0]);
                    close(old_fd[1]);
                }
                if(next)
		{
                   *old_fd = new_fd[0];
		   *(old_fd + 1) = new_fd[1];
		}
                waitfg();
            }
        } else
            execv(attemptPath, cmd->argv); //exec without forking
} /* Exec */


void
waitfg(void)
{
    while(fg_pgid != 0)
        sleep(1);
}

int
push_bg_job(pid_t pid, commandT* cmd)
{
    bgjobL *job = malloc(sizeof(bgjobL));
    if(job)
    {
        job->pid = pid;
        job->cmd = cmd;
        job->next = bgjobs;
        job->prev = NULL;
        if (bgjobs != NULL)
        {
            job->start_position = bgjobs->start_position + 1;
            bgjobs->prev = job;
        }
        else
        {
            job->start_position = 1;
            oldest_bgjob = job;
        }
        bgjobs = job;
        setpgid(pid, pid);
        return 0;
    }
    return -1;
}

void
free_job(bgjobL* job)
{
    free(job);
}

bgjobL*
pop_bg_job(pid_t pid)
{
    bgjobL* prev_job = NULL;
    bgjobL* top_job = bgjobs;
    while(top_job != NULL)
    {
        if (pid == top_job->pid)
        {
            if (prev_job == NULL)
            {
                bgjobs = top_job->next; // first thing on stack
                if(bgjobs)
                    bgjobs->prev = NULL;
                if (top_job == oldest_bgjob)
                    oldest_bgjob = NULL;
            }
            else if(prev_job != NULL && top_job != oldest_bgjob)
            {
                // in the middle of the stack
                prev_job->next = top_job->next;
                top_job->next->prev = prev_job;
            }
            else
            {
                // last thing on stack and not first
                oldest_bgjob = top_job->prev;
                oldest_bgjob->next = NULL;
            }

            top_job->next = NULL;
            top_job->prev = NULL;
            return top_job;
        } else
        {
            prev_job = top_job;
            top_job = top_job->next;
        }
    }
    return NULL;
}

int
size_of_bgjobs(void)
{
    int size = 0;
    bgjobL* top_job = bgjobs;
    while(top_job != NULL){
        size++;
        top_job = top_job->next;
    }
    return size;
}



bool
is_bg(commandT *cmd)
{
    if (strcmp(cmd->argv[(cmd->argc)-1], "&") == 0)
    {
        // last arg is "&"
        free(cmd->argv[(cmd->argc)-1]);
        cmd->argv[(cmd->argc)-1] = 0;
        cmd->argc--;
        return TRUE;
    }
    int last_arg_len = strlen(cmd->argv[(cmd->argc)-1]);
    if (cmd->argv[(cmd->argc)-1][last_arg_len-1] == '&')
    {
        // last char of last arg is "&"
        cmd->argv[(cmd->argc)-1][last_arg_len-1] = '\0';
        return TRUE;
    }
    return FALSE;
}


/*
 * IsBuiltIn
 *
 * arguments:
 *   char *cmd: a command string (e.g. the first token of the command line)
 *
 * returns: bool: TRUE if the command string corresponds to a built-in
 *                command, else FALSE.
 *
 * Checks whether the given string corresponds to a supported built-in
 * command.
 */
	static bool
IsBuiltIn(char* cmd)
{
    // only built in we need to worry about as of now
    if(strcmp( cmd, "cd")==0 || strcmp(cmd, "jobs") == 0 || \
       strcmp(cmd, "fg") == 0 || (strcmp(cmd, "bg") == 0) || \
       strcmp(cmd, "alias") == 0 || strcmp(cmd, "unalias") == 0)
        return TRUE;
    return FALSE;
} /* IsBuiltIn */


/*
 * RunBuiltInCmd
 *
 * arguments:
 *   commandT *cmd: the command to be run
 *
 * returns: none
 *
 * Runs a built-in command.
 */
	static void
RunBuiltInCmd(commandT* cmd)
{
    char *envpath;
    char *path = malloc(sizeof(char) * 256);
    bool do_free = FALSE;
    if(strcmp(cmd->name, "cd")==0)
    {
        do_free = TRUE;
        if(cmd->argc == 1)
        {
            envpath = getenv("HOME"); //get home if no argument
            strncpy(path, envpath, 256);
        }
        else if(cmd->argv[1][0] == '~')
        {
            envpath = getenv("HOME"); //get home if tilde
            strncpy(path, envpath, 128);
            strncat(path, &cmd->argv[1][1], 128); //concatenate rest of arg on home directory
        }
        else
        {
            strncpy(path, cmd->argv[1], 256);
        }
        if(chdir(path))  //chdir returns non-zero if fail
            Print("Directory could not be found.\n");
    }
    else if (strcmp(cmd->name, "jobs") == 0)
    {
        do_free = TRUE;
        bgjobL* prev_job = NULL;
        bgjobL* top_job = oldest_bgjob;
        while(top_job != NULL)
        {
            prev_job = top_job;
            top_job = top_job->prev;
            print_job(prev_job, job_status(prev_job));
        }

    }
    else if (strcmp(cmd->name, "fg") == 0)
    {
        fg(cmd);
    }
    else if (strcmp(cmd->name, "bg") == 0)
    {
        bg(cmd);
    }
    else if (strcmp(cmd->name, "alias") == 0)
    {
        if(cmd->argc == 1)
            print_aliases();
        else
            make_alias(cmd);
    }
    else if (strcmp(cmd->name, "unalias") == 0)
    {
	if(cmd->argc == 2)
		remove_alias(cmd->argv[1]);
    	else
		printf("unalias requires exactly one arg.\n");
    }
	free(path);
    if(do_free)
        freeCommand(cmd);
} /* RunBuiltInCmd */


void
print_aliases(void)
{
    aliasT *top = aliases;
    while(top)
    {
        printf("alias %s='%s'\n", top->lhs, top->rhs);
        top = top->next;
    }
}

void
make_alias(commandT* cmd)
{
    char *lhs = malloc(sizeof(char) * MAXLINE);
    int lhs_size = 0;
    char *rhs = malloc(sizeof(char) * MAXLINE);
    int rhs_size = 0;
    int i = 0;
    bool left = TRUE;
    for(i=0; i<=strlen(cmd->argv[1]); i++)
    {
        char cur_char = cmd->argv[1][i];
        if(left && cur_char != '=')
        {
            lhs[lhs_size] = cur_char;
            lhs_size++;
        }
        else if(left && cur_char == '=')
        {
            lhs[lhs_size] = 0;
            left = FALSE;
        }
        else if (!left && cur_char != '\'')
        {
            rhs[rhs_size] = cur_char;
            rhs_size++;
        }
        else if(!left && cur_char == '\'')
            rhs[rhs_size] = 0;
    }
    aliasT *alias = malloc(sizeof(alias));
    alias->lhs = lhs;
    alias->rhs = rhs;
    push_alias(alias);
}


void
push_alias(aliasT* alias)
{
    aliasT *prev = NULL;
    aliasT *top = aliases;
    while( top && strcmp( alias->lhs, top->lhs) > 0)
    {
	prev = top;
	top = top->next;
    }
    if(prev)
	prev->next = alias;	
    else
	aliases = alias;

    alias->next = top;
}

void
remove_alias(char *name)
{
    aliasT *prev = NULL;
    aliasT *top = aliases;
    while(top)
    {
        if(strcmp(top->lhs, name) == 0)
        {
            if(prev)
                prev->next = top->next;
            else
                aliases = top->next;
            top->next = NULL;
            free(top->lhs);
            free(top->rhs);
            free(top);
            return;
        }
	prev = top;
        top = top->next;
    }
	printf("/bin/bash: line 3: unalias: %s: not found\n", name);
}



void
fg(commandT* cmd)
{
    bgjobL *job = NULL;
    if(cmd->argc == 1)
        job = delete_job_num(0);
    else if (cmd->argc == 2)
        job = delete_job_num(atoi(cmd->argv[1]));
    else
        printf("Error: fg takes max one argument\n");

    if(job)
    {
        fg_pgid = job->pid;
        fg_cmd = job->cmd;
        kill(job->pid, SIGCONT);
        waitfg();
    }
    else if(cmd->argc == 2)
        printf("fg: %s: no such job\n", cmd->argv[1]);
    else
        printf("fg: current: no such job\n");
}

void
bg(commandT* cmd)
{
    bgjobL *job = NULL;
    if(cmd->argc == 1)
        job = continue_job_num(0);
    else if (cmd->argc == 2)
        job = continue_job_num(atoi(cmd->argv[1]));
    else
        printf("Error: bg takes max one argument\n");

    if(job)
        kill(job->pid, SIGCONT);
    else if(cmd->argc == 2)
        printf("bg: %s: no such job\n", cmd->argv[1]);
    else
        printf("bg: current: no such job\n");
}

bgjobL*
continue_job_num(int job_num)
{
    if(job_num == 0)
        return bgjobs;
    else
    {
        bgjobL *top_job = bgjobs;
        while(top_job != NULL)
        {
            if(top_job->start_position == job_num)
                return top_job;
            else
                top_job = top_job->next;
        }
    }
    return NULL;
}

bgjobL*
delete_job_num(int job_num)
{
    if(job_num == 0)
        return pop_bg_job(bgjobs->pid);
    else
    {
        bgjobL *top_job = bgjobs;
        while(top_job != NULL)
        {
            if(top_job->start_position == job_num)
                return pop_bg_job(top_job->pid);
            else
                top_job = top_job->next;
        }
    }
    return NULL;
}



/*
 * CheckJobs
 *
 * arguments: none
 *
 * returns: none
 *
 * Checks the status of running jobs.
 */
	void
CheckJobs()
{
   // loop through stack, if a process is done, pop and print
   bgjobL* prev_job = NULL;
   bgjobL* top_job = oldest_bgjob;
   while(top_job != NULL)
   {
       prev_job = top_job;
       top_job = top_job->prev;
       if(job_status(prev_job) == JOB_DONE)
           print_job(prev_job, JOB_DONE);
   }
} /* CheckJobs */

int
job_status(bgjobL* job)
{
    char* path = malloc(sizeof(char) * MAXLINE);
    sprintf(path, "/proc/%d/status", (int)job->pid);
    struct stat st;
    if (stat(path, &st) == 0)
    {
        FILE *fp = fopen(path, "r");
        free(path);
        char line [ 128 ];
        char status = 0;
        while ( fgets ( line, sizeof line, fp ) != NULL )
        {
            char* line_start = malloc(sizeof(char) * MAXLINE);
            strncpy(line_start, line, 5);
            if (strcmp(line_start, "State") == 0)
            {
                status = line[7];
                break;
            }
        }
        fclose(fp);
        if(status == 'T')
            return JOB_STOPPED;
        else if (status == 'Z')
            printf("ZOMBIE\n");
        else
            return JOB_RUNNING;

        return -1;
    }
    else
    {
        free(path);
        return JOB_DONE;
    }
}

void
print_pid(pid_t pid)
{
    bgjobL* top_job = bgjobs;
    while(top_job)
    {
        if(top_job->pid == pid)
        {
            print_job(top_job, job_status(top_job));
            break;
        }
        else
            top_job = top_job->next;
    }
}

void
print_job(bgjobL* job, const int status)
{
    int is_running = 0;
    char *stat_msg = malloc(sizeof(char) * 20);
    switch(status){
        case 0:
            strcpy(stat_msg, "Running");
            is_running = 1;
            break;
        case 1:
            strcpy(stat_msg, "Stopped");
            break;
        case 2:
            strcpy(stat_msg, "Done");
            break;
    }
    printf("[%d] %s ", job->start_position, stat_msg);
    int i = 0;
    for(i=0; i < job->cmd->argc; i++)
    {
        if(strchr(job->cmd->argv[i], ' '))
        {
            printf("\"");
            printf("%s", job->cmd->argv[i]);
            printf("\"");
        }
        else
            printf("%s ", job->cmd->argv[i]);
    }
    if(is_running == 1)
        printf(" &");
    printf("\n");
    fflush(stdout);
    if (status == 2)
    {
        pop_bg_job(job->pid);
        free_job(job);
    }
    free(stat_msg);
}


/*
 * FileExists
 *
 * arguments: filename to check
 *
 * returns: int(bool) indicating existence
 *
 * Checks if a file exists by trying to open it
 */

int FileExists(char *fname)
{
	FILE *file;
	if ((file = fopen(fname, "r")))
	{
		fclose(file);
		return 1;
	}
	return 0;
}

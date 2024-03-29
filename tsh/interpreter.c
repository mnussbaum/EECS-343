/***************************************************************************
 *  Title: Input
 * -------------------------------------------------------------------------
 *    Purpose: Handles the input from stdin
 *    Author: Stefan Birrer
 *    Version: $Revision: 1.4 $
 *    Last Modification: $Date: 2009/10/12 20:50:12 $
 *    File: $RCSfile: interpreter.c,v $
 *    Copyright: (C) 2002 by Stefan Birrer
 ***************************************************************************/
/***************************************************************************
 *  ChangeLog:
 * -------------------------------------------------------------------------
 *    $Log: interpreter.c,v $
 *    Revision 1.4  2009/10/12 20:50:12  jot836
 *    Commented tsh C files
 *
 *    Revision 1.3  2009/10/11 04:45:50  npb853
 *    Changing the identation of the project to be GNU.
 *
 *    Revision 1.2  2008/10/10 00:12:09  jot836
 *    JSO added simple command line parser to interpreter.c. It's not pretty
 *    but it works. Handles quoted strings, and preserves backslash behavior
 *    of bash. Also, added simple skeleton code as well as code to create and
 *    free commandT structs given a command line.
 *
 *    Revision 1.1  2005/10/13 05:24:59  sbirrer
 *    - added the skeleton files
 *
 *    Revision 1.4  2002/10/24 21:32:47  sempi
 *    final release
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
#define __INTERPRETER_IMPL__

/************System include***********************************************/
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include "string.h"
#include <ctype.h>

/************Private include**********************************************/
#include "interpreter.h"
#include "io.h"
#include "runtime.h"

/************Defines and Typedefs*****************************************/
/*  #defines and typedefs should have their names in all caps.
 *  Global variables begin with g. Global constants with k. Local
 *  variables should be in all lower case. When initializing
 *  structures and arrays, line everything up in neat columns.
 */
typedef struct string_l
{
  char* s;
  struct string_l* next;
} stringL;

int BUFSIZE = 512;
int MAXARGS = 100;

/**************Function Prototypes******************************************/

commandT*
getCommand(char* cmdLine);

commandT_list*
get_command_list(char* cmdLine);

void
freeCommand(commandT* cmd);

bool
HandleEnvironment(commandT* cmd);

bool
isComment(commandT* cmd);
/**************Implementation***********************************************/


/*
 * Interpret
 *
 * arguments:
 *   char *cmdLine: pointer to the command line string
 *
 * returns: none
 *
 * This is the high-level function called by tsh's main to interpret a
 * command line.
 */
void
Interpret(char* cmdLine)
{

   /* else if(strchr(cmdLine, '>') || strchr(cmdLine, '<'))
    {
        commandT* cmd = getCommand(cmdLine);
        if(strchr(cmdLine, '>') > strchr(cmdLine, '<'))
            RunCmdRedirOut(cmd,);
        else
            RunCmdRedirIn(cmd);
    }*/

    if(strchr(cmdLine, '|'))
    {
        commandT_list* commands = get_command_list(cmdLine);
        RunCmdPipe(commands);
    }
    else
    {
        commandT* cmd = getCommand(cmdLine);

        if(!isComment(cmd)) //check for comments
        {
            if(!HandleEnvironment(cmd)) //check if environment update
                RunCmd(cmd);  //otherwise run the command
            else
                freeCommand(cmd);
        }
        else
            freeCommand(cmd);
    }
} /* Interpret */


commandT_list*
get_command_list(char* cmdLine)
{
    commandT_list* commands = NULL;
    commandT_list* last_command = NULL;
    char* cmd_buf = malloc(sizeof(char) * MAXLINE);
    int cmd_buf_len = 0;
    int i = 0;
    for (i=0; i <= strlen(cmdLine); i++)
    {
        char cur_char = cmdLine[i];
        if(cur_char == '|' || i == strlen(cmdLine))
        {
            commandT_list* command_cell = malloc(sizeof(commandT_list));
            cmd_buf[cmd_buf_len] = 0;
            commandT* cmd = getCommand(cmd_buf);
            memset(cmd_buf, 0, sizeof(char)*MAXLINE);
            cmd_buf_len = 0;
            command_cell->cmd = cmd;
            command_cell->next = NULL;
            if(commands == NULL)
                commands = command_cell;
            else if(last_command != NULL)
                last_command->next = command_cell;
            last_command = command_cell;
        }
        else
        {
            cmd_buf[cmd_buf_len] = cur_char;
            cmd_buf_len++;
        }
    }
    free(cmd_buf);
    return commands;
}

/*
 * getCommand
 *
 * arguments:
 *   char *cmdLine: pointer to the command line string
 *
 * returns: commandT*: pointer to the commandT struct generated by
 *                     parsing the cmdLine string
 *
 * This parses the command line string, and returns a commandT struct,
 * as defined in runtime.h.  You must free the memory used by commandT
 * using the freeCommand function after you are finished.
 *
 * This function tokenizes the input, preserving quoted strings. It
 * supports escaping quotes and the escape character, '\'.
 */
commandT*
getCommand(char* cmdLine)
{
  commandT* cmd = malloc(sizeof(commandT) + sizeof(char*) * MAXARGS);
  cmd->argv[0] = 0;
  cmd->name = 0;
  cmd->argc = 0;

  int i, inArg = 0;
  char quote = 0;
  char escape = 0;

  // Set up the initial empty argument
  char* tmp = malloc(sizeof(char*) * BUFSIZE);
  char* envtmp = malloc(sizeof(char*) * BUFSIZE);
  int tmpLen = 0;
  tmp[0] = 0;

  //printf("parsing:%s\n", cmdLine);

  for (i = 0; cmdLine[i] != 0; i++)
    {
      //printf("\tindex %d, char %c\n", i, cmdLine[i]);

      // Check for whitespace
      if (cmdLine[i] == ' ')
        {
          if (inArg == 0)
            continue;
          if (quote == 0)
            {
              // End of an argument
              //printf("\t\tend of argument %d, got:%s\n", cmd.argc, tmp);
             /* Remove new line chars */
	      cmd->argv[cmd->argc] = malloc(sizeof(char) * (tmpLen + 1));

	      if(tmp[0] == '$')
	      {
		strncpy(envtmp, tmp+1,BUFSIZE-1);
		char *envtmp1 = getenv(envtmp);
	        if(envtmp1 != NULL)
		{
			cmd->argv[cmd->argc] = realloc(cmd->argv[cmd->argc], strlen(envtmp1));
			strcpy(cmd->argv[cmd->argc], envtmp1);

		}
		else
			cmd->argv[cmd->argc][0] = 0;
	      }
	      else
	      	strcpy(cmd->argv[cmd->argc], tmp);

              inArg = 0;
              tmp[0] = 0;
              tmpLen = 0;
              cmd->argc++;
              cmd->argv[cmd->argc] = 0;
              continue;
            }
        }

      // If we get here, we're in text or a quoted string
      inArg = 1;

      // Start or end quoting.
      if (cmdLine[i] == '\'' || cmdLine[i] == '"')
        {
          if (escape != 0 && quote != 0 && cmdLine[i] == quote)
            {
              // Escaped quote. Add it to the argument.
              tmp[tmpLen++] = cmdLine[i];
              tmp[tmpLen] = 0;
              escape = 0;
              continue;
            }

          if (quote == 0)
            {
              //printf("\t\tstarting quote around %c\n", cmdLine[i]);
              quote = cmdLine[i];
              continue;
            }
          else
            {
              if (cmdLine[i] == quote)
                {
                  //printf("\t\tfound end quote %c\n", quote);
                  quote = 0;
                  continue;
                }
            }
        }

      // Handle escape character repeat
      if (cmdLine[i] == '\\' && escape == '\\')
        {
          escape = 0;
          tmp[tmpLen++] = '\\';
          tmp[tmpLen] = 0;
          continue;
        }

      // Handle single escape character followed by a non-backslash or quote character
      if (escape == '\\')
        {
          if (quote != 0)
            {
              tmp[tmpLen++] = '\\';
              tmp[tmpLen] = 0;
            }
          escape = 0;
        }

      // Set the escape flag if we have a new escape character sequence.
      if (cmdLine[i] == '\\')
        {
          escape = '\\';
          continue;
        }

      tmp[tmpLen++] = cmdLine[i];
      tmp[tmpLen] = 0;
    }
  // End the final argument, if any.
  if (tmpLen > 0)
    {
      //printf("\t\tend of argument %d, got:%s\n", cmd.argc, tmp);
      cmd->argv[cmd->argc] = malloc(sizeof(char) * (tmpLen + 1));

      if(tmp[0] == '$')
      {
	strncpy(envtmp,tmp+1,BUFSIZE - 1);
	char *envtmp2 = getenv(envtmp);
      	if(envtmp2 != NULL)
	{
		cmd->argv[cmd->argc] = realloc(cmd->argv[cmd->argc], strlen(envtmp2));
		strcpy(cmd->argv[cmd->argc], envtmp2);
      	}
	else
		cmd->argv[cmd->argc][0] = 0;
      }
      else
	strcpy(cmd->argv[cmd->argc], tmp);

      inArg = 0;
      tmp[0] = 0;
      tmpLen = 0;
      cmd->argc++;
      cmd->argv[cmd->argc] = 0;
    }

  free(envtmp);
  free(tmp);
  cmd->name = cmd->argv[0];

  return cmd;
} /* getCommand */


/*
 * freeCommand
 *
 * arguments:
 *   commandT *cmd: pointer to the commandT struct to be freed
 *
 * returns: none
 *
 * This function frees all the memory associated with the given
 * commandT struct, before freeing the struct's memory itself.
 */
void
freeCommand(commandT* cmd)
{
  int i;

  cmd->name = 0;
  for (i = 0; cmd->argv[i] != 0; i++)
    {
      free(cmd->argv[i]);
      cmd->argv[i] = 0;
    }
  free(cmd);
} /* freeCommand */

/*
 * HandleEnvironment
 * arguments
 *  commandT *cmd: pointer to the commandT struct to be checked
 *
 *  returns: boolean indicating whether the environment changed
 *
 *  Function checks if a command is setting an environment variable
 */
bool
HandleEnvironment(commandT* cmd)
{
	if(cmd->argc != 1)
		return FALSE;
	int envCommandSection = 0;
	bool validSyntax = FALSE;
	bool specialCase = FALSE;
	char *envCommandArray[2];
	envCommandArray[0] = malloc(64 * sizeof(char));
	envCommandArray[1] = malloc(64 * sizeof(char));
	size_t const len = strlen(cmd->argv[0])+1;
 	char *envCommand = memcpy(malloc(len), cmd->argv[0], len); //copy name into new memory
	char *envToken = strtok(envCommand,"="); //break up envCommand at =
	while(envToken != NULL) //parse through it
	{
		if(envCommandSection > 1) //if we hit this, there are multiple = signs
		{
			validSyntax = FALSE;
			break;
		}
		else if(envCommandSection == 1) //if we hit this, there is one equal sign
			validSyntax = TRUE;
		strncpy(envCommandArray[envCommandSection],envToken, 64); //copy substring into new array
		envCommandSection++;
		envToken = strtok(NULL, "="); //get next substring
	}
	//handle the special case where the variable was set to null e.g. $PS1=''
	//strtok ignores strings that end in delimiters
	if(cmd->argv[0][len-2] == '=' && envCommandSection == 1)
	{
		validSyntax = TRUE;
		specialCase = TRUE;
	}

	int i;
	for(i= 0;i<strlen(envCommandArray[0]);i++) // loop through variable name, make sure no lower case
	{
		if(!(isupper(envCommandArray[0][i]) || isdigit(envCommandArray[0][i])))
		{
			validSyntax = FALSE;
			break;
		}
	}
	if((validSyntax) && !(specialCase)) //if correct normal assignment, setenv
		setenv(envCommandArray[0], envCommandArray[1], 1);
	else if(validSyntax && specialCase) //if correct null asignment, unsetenv
		unsetenv(envCommandArray[0]);

	free(envCommand);
	free(envCommandArray[0]);
	free(envCommandArray[1]);

	return validSyntax;
}

/*
 * isComment
 * arguments
 * commandT *cmd: pointer to the commandT struct to be checked
 *
 * returns boolean indicating whether the command was a comment
 *
 * Function checks if command is a commant and starts with #
 */
bool
isComment(commandT *cmd)
{
	if(cmd->argc != 0) //check if there is an argument and it starts with #
		return cmd->argv[0][0] == '#';
	return FALSE;
}

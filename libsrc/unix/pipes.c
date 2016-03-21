#include "quip_config.h"

#ifdef HAVE_POPEN

/** open a subprocess */

#include <stdio.h>
#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include "quip_prot.h"

ITEM_INTERFACE_DECLARATIONS(Pipe,pipe)

static void creat_pipe(QSP_ARG_DECL  const char *name, const char* command, const char* rw)
{
	Pipe *pp;
	int flg;

	if( *rw == 'r' ) flg=READ_PIPE;
	else if( *rw == 'w' ) flg=WRITE_PIPE;
	else {
		sprintf(ERROR_STRING,"create_pipe:  bad r/w string \"%s\"",rw);
		WARN(ERROR_STRING);
		return;
	}

	pp = new_pipe(QSP_ARG  name);
	if( pp == NO_PIPE ) return;

	pp->p_cmd = savestr(command);
	pp->p_flgs = flg;
	pp->p_fp = popen(command,rw);

	if( pp->p_fp == NULL ){
		sprintf(ERROR_STRING,
			"unable to execute command \"%s\"",command);
		WARN(ERROR_STRING);
		close_pipe(QSP_ARG  pp);
	}
}

void close_pipe(QSP_ARG_DECL  Pipe *pp)
{
	if( pp->p_fp != NULL && pclose(pp->p_fp) == -1 ){
		sprintf(ERROR_STRING,"Error closing pipe \"%s\"!?",pp->p_name);
		WARN(ERROR_STRING);
	}
	del_pipe(QSP_ARG  pp);
	rls_str(pp->p_name);
	rls_str(pp->p_cmd);
}

static void sendto_pipe(QSP_ARG_DECL  Pipe *pp,const char* text)
{
	if( (pp->p_flgs & WRITE_PIPE) == 0 ){
		sprintf(ERROR_STRING,"Can't write to read pipe %s",pp->p_name);
		WARN(ERROR_STRING);
		return;
	}

	if( fprintf(pp->p_fp,"%s\n",text) == EOF ){
		sprintf(ERROR_STRING,
			"write failed on pipe \"%s\"",pp->p_name);
		WARN(ERROR_STRING);
		close_pipe(QSP_ARG  pp);
	} else if( fflush(pp->p_fp) == EOF ){
		sprintf(ERROR_STRING,
			"fflush failed on pipe \"%s\"",pp->p_name);
		WARN(ERROR_STRING);
		close_pipe(QSP_ARG  pp);
	}
#ifdef DEBUG
	else if( debug ) advise("pipe flushed");
#endif /* DEBUG */
}

static void readfr_pipe(QSP_ARG_DECL  Pipe *pp,const char* varname)
{
	char buf[LLEN];

	if( (pp->p_flgs & READ_PIPE) == 0 ){
		sprintf(ERROR_STRING,"Can't read from  write pipe %s",pp->p_name);
		WARN(ERROR_STRING);
		return;
	}

	if( fgets(buf,LLEN,pp->p_fp) == NULL ){
		if( verbose ){
			sprintf(ERROR_STRING,
		"read failed on pipe \"%s\"",pp->p_name);
			advise(ERROR_STRING);
		}
		close_pipe(QSP_ARG  pp);
		ASSIGN_VAR(varname,"pipe_read_error");
	} else {
		/* remove trailing newline */
		if( buf[strlen(buf)-1] == '\n' )
			buf[strlen(buf)-1] = 0;
		ASSIGN_VAR(varname,buf);
	}
}

#define N_RW_CHOICES	2

// we have to pass "r" or "w" to popen, but it is nice to have
// the more human-readable complete strings in the scripts...

static const char *rw_choices[N_RW_CHOICES]={"read","write"};

static COMMAND_FUNC( do_newpipe )
{
	const char *pipe_name;
	const char *cmd;
	const char *mode;
	int n;

	pipe_name=NAMEOF("name for pipe");

	n=WHICH_ONE("read/write",N_RW_CHOICES,rw_choices);

	cmd=NAMEOF("command");

	if( n < 0 || n >= 2 ) return;
	if( n == 0 ) mode="r";
	else mode="w";
	
	creat_pipe(QSP_ARG  pipe_name,cmd,mode);
}

static COMMAND_FUNC( do_sendpipe )
{
	Pipe *pp;
	const char *s;

	pp=PICK_PIPE("");
	s=NAMEOF("text to send");

	if( pp == NO_PIPE ) return;
	sendto_pipe(QSP_ARG  pp,s);
}

static COMMAND_FUNC( do_readpipe )
{
	Pipe *pp;
	const char *s;

	pp=PICK_PIPE("");
	s=NAMEOF("variable for text storage");

	if( pp == NO_PIPE ) {
		ASSIGN_VAR(s,"error_missing_pipe");
		return;
	}
	readfr_pipe(QSP_ARG  pp,s);
}

static COMMAND_FUNC( do_pipe_info )
{
	Pipe *pp;
	int i;

	pp = PICK_PIPE("");
	if( pp == NO_PIPE ) return;

	if( pp->p_flgs & READ_PIPE ) i=0;
	else if( pp->p_flgs & READ_PIPE ) i=1;
#ifdef CAUTIOUS
	else {
		WARN("CAUTIOUS:  bad pipe r/w flag");
		return;
	}
#endif /* CAUTIOUS */

	sprintf(msg_str,"Pipe:\t\t\"%s\", %s",pp->p_name,rw_choices[i]);
	prt_msg(msg_str);
	sprintf(msg_str,"Command:\t\t\"%s\"",pp->p_cmd);
	prt_msg(msg_str);
}

static COMMAND_FUNC( do_closepipe )
{
	Pipe *pp;

	pp = PICK_PIPE("");
	if( pp == NO_PIPE ) return;

	close_pipe(QSP_ARG  pp);
}

static COMMAND_FUNC( do_list_pipes ){ list_pipes(SINGLE_QSP_ARG); }

/* fp should be null - but where do we specify the pipe handle? */

static COMMAND_FUNC( do_pipe_redir )
{
	Pipe *pp;

	pp = PICK_PIPE("");
	if( pp == NO_PIPE ) return;

	if( (pp->p_flgs & READ_PIPE) == 0 ) {
		sprintf(ERROR_STRING,
	"do_pipe_redir:  pipe %s is not readable!?",pp->p_name);
		WARN(ERROR_STRING);
		return;
	}

	//push_input_file(QSP_ARG  msg_str);
	sprintf(msg_str,"Pipe \"%s\"",pp->p_cmd);
	redir(QSP_ARG pp->p_fp, msg_str);
	SET_QRY_DUPFILE(CURR_QRY(THIS_QSP) , (FILE *) pp );
	SET_QRY_FLAG_BITS(CURR_QRY(THIS_QSP), Q_PIPE);
}

#define ADD_CMD(s,f,h)	ADD_COMMAND(pipes_menu,s,f,h)

MENU_BEGIN(pipes)
ADD_CMD( open,		do_newpipe,	create new pipe )
ADD_CMD( sendto,	do_sendpipe,	send input text to pipe )
ADD_CMD( read,		do_readpipe,	read output text from pipe )
ADD_CMD( close,		do_closepipe,	close pipe )
ADD_CMD( list,		do_list_pipes,	list currently open pipes )
ADD_CMD( info,		do_pipe_info,	give information about a pipe )
ADD_CMD( redir,		do_pipe_redir,	redirect interpreter to a readable pipe )
MENU_END(pipes)

static double pipe_exists(QSP_ARG_DECL  const char *s)
{
	Pipe *pp;

	pp=pipe_of(QSP_ARG  s);
	if( pp==NO_PIPE ) return(0.0);
	else return(1.0);
}

COMMAND_FUNC( do_pipe_menu )
{
	static int inited=0;
	if( ! inited ){
		DECLARE_STR1_FUNCTION(	pipe_exists,	pipe_exists )
		inited=1;
	}
	PUSH_MENU(pipes);
}


#endif /* HAVE_POPEN */


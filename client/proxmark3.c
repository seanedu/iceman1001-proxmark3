//-----------------------------------------------------------------------------
// Copyright (C) 2009 Michael Gernoth <michael at gernoth.net>
// Copyright (C) 2010 iZsh <izsh at fail0verflow.com>
//
// This code is licensed to you under the terms of the GNU GPL, version 2 or,
// at your option, any later version. See the LICENSE.txt file for the text of
// the license.
//-----------------------------------------------------------------------------
// Main binary
//-----------------------------------------------------------------------------

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <readline/readline.h>
#include <readline/history.h>

#include "proxmark3.h"
#include "proxgui.h"
#include "cmdmain.h"
#include "uart.h"
#include "ui.h"
#include "sleep.h"
#include "cmdparser.h"
#include "cmdhw.h"
#include "whereami.h"


// a global mutex to prevent interlaced printing from different threads
pthread_mutex_t print_lock;

static serial_port sp;
static UsbCommand txcmd;
volatile static bool txcmd_pending = false;

void SendCommand(UsbCommand *c) {
	#if 0
	printf("Sending %d bytes\n", sizeof(UsbCommand));
	#endif

	if (offline) {
		PrintAndLog("Sending bytes to proxmark failed - offline");
		return;
	}
	/**
	The while-loop below causes hangups at times, when the pm3 unit is unresponsive
	or disconnected. The main console thread is alive, but comm thread just spins here.
	Not good.../holiman
	**/
	while(txcmd_pending);

	txcmd = *c;
	txcmd_pending = true;
}

struct receiver_arg {
	int run;
};

struct main_loop_arg {
	int usb_present;
	char *script_cmds_file;
};

byte_t rx[0x1000000];
byte_t* prx = rx;

// static void showBanner(void){
	// printf("██████╗ ███╗   ███╗ ████╗     ...Iceman fork\n");
	// printf("██╔══██╗████╗ ████║   ══█║\n");
	// printf("██████╔╝██╔████╔██║ ████╔╝\n");
	// printf("██╔═══╝ ██║╚██╔╝██║   ══█║    iceman@icesql.net\n");
	// printf("██║     ██║ ╚═╝ ██║ ████╔╝ https://github.com/iceman1001/proxmark3\n");
	// printf("╚═╝     ╚═╝     ╚═╝ ╚═══╝v1.7.0\n");
// }


static void *uart_receiver(void *targ) {
	struct receiver_arg *arg = (struct receiver_arg*)targ;
	size_t rxlen;
	size_t cmd_count;

	while (arg->run) {

		rxlen = sizeof(UsbCommand);

		if (uart_receive(sp, prx, &rxlen)) {
			prx += rxlen;
			if (((prx-rx) % sizeof(UsbCommand)) != 0)
				continue;
			
			cmd_count = (prx-rx) / sizeof(UsbCommand);

			for (size_t i = 0; i < cmd_count; i++)
				UsbCommandReceived((UsbCommand*)( rx + ( i * sizeof(UsbCommand))));

		}
		prx = rx;

		if (txcmd_pending) {
			bool res = uart_send(sp, (byte_t*) &txcmd, sizeof(UsbCommand));
			if (!res) {
				PrintAndLog("Sending bytes to proxmark failed");
			}
			txcmd_pending = false;
		}
	}
	pthread_exit(NULL);
	return NULL;
}

static void *main_loop(void *targ) {
	struct main_loop_arg *arg = (struct main_loop_arg*)targ;
	struct receiver_arg rarg;
	char *cmd = NULL;
	pthread_t reader_thread;
  
	if (arg->usb_present == 1) {
		rarg.run = 1;
		pthread_create(&reader_thread, NULL, &uart_receiver, &rarg);
		// cache Version information now:
		CmdVersion(NULL);
	}

	FILE *script_file = NULL;
	char script_cmd_buf[256] = {0x00};  // iceman, needs lua script the same file_path_buffer as the rest

	if (arg->script_cmds_file) {
		script_file = fopen(arg->script_cmds_file, "r");
		
		if (script_file)
			printf("using 'scripting' commands file %s\n", arg->script_cmds_file);
	}

	read_history(".history");

	while(1)  {

		// If there is a script file
		if (script_file) {
			
			if (!fgets(script_cmd_buf, sizeof(script_cmd_buf), script_file)) {
				fclose(script_file);
				script_file = NULL;
			} else {
				char *nl;
				nl = strrchr(script_cmd_buf, '\r');
				if (nl)
					*nl = '\0';
				
				nl = strrchr(script_cmd_buf, '\n');
				
				if (nl)
					*nl = '\0';
				
				int newlen = strlen(script_cmd_buf);
				if ((cmd = (char*) malloc( newlen + 1)) != NULL) {
					memset(cmd, 0x00, newlen);
					strcpy(cmd, script_cmd_buf);
					printf("%s\n", cmd);
				}
			}
		} else {
			cmd = readline(PROXPROMPT);
		}
		
		// this one should pick up all non-null cmd...
		// why is there a 
		if (cmd) {
			if (strlen(cmd) > 0) {
				while(cmd[strlen(cmd) - 1] == ' ')
					cmd[strlen(cmd) - 1] = 0x00;
			}

			if (cmd[0] != 0x00) {
				int ret = CommandReceived(cmd);
				add_history(cmd);
				
				// exit or quit
				if (ret == 99) 
					break;
			}
			free(cmd);
			cmd = 0;
		} else {
			printf("\n");
			break;
		}
	}

	if (script_file) {
		fclose(script_file);
	}
	
	write_history(".history");

	free(cmd);
	cmd = 0;
			
	if (arg->usb_present == 1) {
		rarg.run = 0;
		pthread_join(reader_thread, NULL);
	}

	ExitGraphics();
	pthread_exit(NULL);
	return NULL;
}

static void dumpAllHelp(int markdown)
{
	printf("\n%sProxmark3 command dump%s\n\n",markdown?"# ":"",markdown?"":"\n======================");
	printf("Some commands are available only if a Proxmark is actually connected.%s\n",markdown?"  ":"");
	printf("Check column \"offline\" for their availability.\n");
	printf("\n");
	command_t *cmds = getTopLevelCommandTable();
	dumpCommandsRecursive(cmds, markdown);
}

static char *my_executable_path = NULL;
static char *my_executable_directory = NULL;

const char *get_my_executable_path(void)
{
	return my_executable_path;
}

const char *get_my_executable_directory(void)
{
	return my_executable_directory;
}

static void set_my_executable_path(void)
{
	int path_length = wai_getExecutablePath(NULL, 0, NULL);
	if (path_length != -1) {
		my_executable_path = (char*)malloc(path_length + 1);
		int dirname_length = 0;
		if (wai_getExecutablePath(my_executable_path, path_length, &dirname_length) != -1) {
			my_executable_path[path_length] = '\0';
			my_executable_directory = (char *)malloc(dirname_length + 2);
			strncpy(my_executable_directory, my_executable_path, dirname_length+1);
			my_executable_directory[dirname_length+1] = '\0';
		}
	}
}

int main(int argc, char* argv[]) {
	srand(time(0));
  
	if (argc < 2) {
		printf("syntax: %s <port>\n\n",argv[0]);
		printf("\tLinux example:'%s /dev/ttyACM0'\n\n", argv[0]);
		printf("help:   %s -h\n\n", argv[0]);
		printf("\tDump all interactive help at once\n");
		printf("markdown:   %s -m\n\n", argv[0]);
		printf("\tDump all interactive help at once in markdown syntax\n");
		return 1;
	}
	if (strcmp(argv[1], "-h") == 0) {
		printf("syntax: %s <port>\n\n",argv[0]);
		printf("\tLinux example:'%s /dev/ttyACM0'\n\n", argv[0]);
		dumpAllHelp(0);
		return 0;
	}
	if (strcmp(argv[1], "-m") == 0) {
		dumpAllHelp(1);
		return 0;
	}

	set_my_executable_path();
	
	// Make sure to initialize
	struct main_loop_arg marg = {
		.usb_present = 0,
		.script_cmds_file = NULL
	};
	
	pthread_t main_loop_threat;

  	sp = uart_open(argv[1]);
	if (sp == INVALID_SERIAL_PORT) {
		printf("ERROR: invalid serial port\n");
		marg.usb_present = 0;
		offline = 1;
	} else if (sp == CLAIMED_SERIAL_PORT) {
		printf("ERROR: serial port is claimed by another process\n");
		marg.usb_present = 0;
		offline = 1;
	} else {
		marg.usb_present = 1;
		offline = 0;
	}

	// If the user passed the filename of the 'script' to execute, get it
	if (argc > 2 && argv[2]) {
		if (argv[2][0] == 'f' &&  //buzzy, if a word 'flush' passed, flush the output after every log entry.
			argv[2][1] == 'l' &&
			argv[2][2] == 'u' &&
			argv[2][3] == 's' &&
			argv[2][4] == 'h')
		{
			printf("Output will be flushed after every print.\n");
			flushAfterWrite = 1;
		}
		else {
			marg.script_cmds_file = argv[2];
		}
	}

	// create a mutex to avoid interlacing print commands from our different threads
	pthread_mutex_init(&print_lock, NULL);

	pthread_create(&main_loop_threat, NULL, &main_loop, &marg);
	InitGraphics(argc, argv);

	MainGraphics();

	pthread_join(main_loop_threat, NULL);

	// Clean up the port
	if (offline == 0)
		uart_close(sp);
  
	// clean up mutex
	pthread_mutex_destroy(&print_lock);
	
	exit(0);
}

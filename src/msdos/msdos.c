#include "driver.h"
#include "mamalleg.h"
#include <dos.h>
#include <signal.h>
#include <time.h>
#include <ctype.h>
#include "ticker.h"
#include "zvgintrf.h"

int msdos_init_sound(void);
int msdos_init_input(void);
void msdos_shutdown_sound(void);
void msdos_shutdown_input(void);
extern void cli_frontend_exit( void );
extern int cli_frontend_init( int argc, char **argv );

/* avoid wild card expansion on the command line (DJGPP feature) */
char **__crt0_glob_function(void)
{
	return 0;
}

/* put here cleanup routines to be executed when the program is terminated. */
void osd_exit(void)
{
	zvg_close();
	msdos_shutdown_sound();
	msdos_shutdown_input();
	allegro_exit();
}



static void signal_handler(int num)
{
	cli_frontend_exit();
	osd_exit();
	ScreenClear();
	ScreenSetCursor( 0, 0 );
	if( num == SIGINT )
	{
		cpu_dump_states();
	}

	signal(num, SIG_DFL);
	raise(num);
}

/* put here anything you need to do when the program is started. Return 0 if */
/* initialization was successful, nonzero otherwise. */
int osd_init(void)
{
	int result = msdos_init_sound();
	if( result == 0 )
	{
		result = msdos_init_input();
	}
	if( result == 0 )
	{
		result = zvg_open();
	}

	add_exit_callback( osd_exit );

	return result;
}


//============================================================
//	osd_alloc_executable
//============================================================

void *osd_alloc_executable(size_t size)
{
	return malloc( size );
}



//============================================================
//	osd_free_executable
//============================================================

void osd_free_executable(void *ptr)
{
	free( ptr );
}



//============================================================
//	osd_is_bad_read_ptr
//============================================================

int osd_is_bad_read_ptr(const void *ptr, size_t size)
{
	return 0;
}



#ifdef MESS

int osd_select_file(mess_image *img, char *filename)
{
	return 0;
}

void osd_image_load_status_changed(mess_image *img, int is_final_unload)
{
}

void osd_parallelize(void (*task)(void *param, int task_num, int task_count), void *param, int max_tasks)
{
	task(param, 0, 1);
}

void osd_begin_final_unloading( void )
{
}

int osd_trying_to_quit( void )
{
	return 0;
}

void osd_config_save_xml(int type, struct _mame_file *file)
{
}

//============================================================
//	osd_keyboard_disabled
//============================================================

int osd_keyboard_disabled(void)
{
	return 0;
}

#endif

int main (int argc, char **argv)
{
	int game_index;
	int res = 0;

	allegro_init();

	/* Allegro changed the signal handlers... change them again to ours, to */
	/* avoid the "Shutting down Allegro" message which confuses users into */
	/* thinking crashes are caused by Allegro. */
	signal(SIGABRT, signal_handler);
	signal(SIGFPE,  signal_handler);
	signal(SIGILL,  signal_handler);
	signal(SIGSEGV, signal_handler);
	signal(SIGTERM, signal_handler);
	signal(SIGINT,  signal_handler);
	signal(SIGKILL, signal_handler);
	signal(SIGQUIT, signal_handler);

	init_ticker();	/* after Allegro init because we use cpu_cpuid */

	game_index = cli_frontend_init( argc, argv );

	if( game_index != -1 )
	{
		/* go for it */
		res = run_game (game_index);
	}

	cli_frontend_exit();

	exit (res);
}

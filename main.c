#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>

#ifdef SUN
#include <crypt.h>
#endif

#include <SDL/SDL.h>
#include <SDL/SDL_thread.h>
#include "font.h"

/* PAM */
#include <security/pam_appl.h>
//#include <security/pam_misc.h>

#include "SDL_endian.h"

#if SDL_BYTEORDER == SDL_LIL_ENDIAN
#define SWAP16(X)    (X)
#define SWAP32(X)    (X)
#else
#define SWAP16(X)    SDL_Swap16(X)
#define SWAP32(X)    SDL_Swap32(X)
#endif

#define DEBUG 1
int main (int argc, char *argv[]);
int draw_func (void *screen);
void DrawPixel(SDL_Surface *screen, int x, int y, Uint8 R, Uint8 G, Uint8 B);
void draw_rect (SDL_Surface *screen, int x1, int y1, int x2, int y2);

int eventloop (void *screen);

struct pampass_data
  {
  char *pd_username;                  /* username */
  char *pd_password;                   /* password */
  };



/* main */
int main (int argc, char *argv[])
{
	SDL_Surface *screen;
	SDL_Thread *thread;
	SDL_Thread *eventthread;

	if ( SDL_Init (SDL_INIT_AUDIO | SDL_INIT_VIDEO) < 0 ) {
		fprintf (stderr, "Unable to initalize SDL: %s\n", SDL_GetError());
		exit (1);
	}
	atexit (SDL_Quit);

	screen = SDL_SetVideoMode (640, 480, 16, SDL_SWSURFACE /*| SDL_FULLSCREEN*/);
	if ( screen == NULL ) {
		// Just incase we can't do fullscreen!
		screen = SDL_SetVideoMode (640, 480, 16, SDL_SWSURFACE);

		if ( screen == NULL ) {
			fprintf ( stderr, "Unable to set 640x480 video: %s\n", SDL_GetError());
			exit (1);	
		}
	}

	thread=SDL_CreateThread(draw_func, screen);	
	if ( thread == NULL ) {
		fprintf (stderr, "Unable to create thread: %s\n", SDL_GetError());
		exit (1);
	}
	eventthread=SDL_CreateThread(eventloop, screen);	
	if ( eventthread == NULL ) {
		fprintf (stderr, "Unable to create thread: %s\n", SDL_GetError());
		exit (1);
	}

	/* Exit when our threads do */
	SDL_WaitThread (eventthread, NULL);	
	return 0;
}

/* Our drawing thread */
int draw_func (void *screen)
{
	int x, y;
	Uint8 r, g, b;
	struct SDLFont *font1;
	char *string="XDM Login.";

	r=(Uint8) 0xDA;
	g=(Uint8) 0xDA;
	b=(Uint8) 0xDA;

	font1=initFont ("data/font1", 0, 0, 0, 1);

	for ( y=0; y < 480; y++) {
		for ( x=0; x < 640; x++) {
			DrawPixel ((SDL_Surface *) screen, x, y, r, g, b); 
		}
	}
	
	drawString ((SDL_Surface *) screen, font1, (int) (640/2)-(stringWidth(font1, string)/2), (int) 480/5, string);
	freeFont (font1);

	draw_rect ((SDL_Surface *) screen, 640/2, 480/2, (640/2)+(stringWidth(font1, "m")*8), (480/2)+30);
	draw_rect ((SDL_Surface *) screen, 640/2, 480/2+40, (640/2)+(stringWidth(font1, "m")*8), (480/2)+70);

	SDL_Flip ((SDL_Surface *) screen);
}

void draw_rect (SDL_Surface *screen, int x1, int y1, int x2, int y2)
{
	int x, y;
	for ( y=y1; y < y2; y++ ) {
		for ( x=x1; x < x2; x++ ) {
			DrawPixel ((SDL_Surface *) screen, x, y, 0xFF, 0xFF, 0xFF); 
		}
	}
}

/* From the SDL tutorial, i'm lazy. */
void DrawPixel(SDL_Surface *screen, int x, int y,
Uint8 R, Uint8 G,
Uint8 B)
{
    Uint32 color = SDL_MapRGB(screen->format, R, G, B);

    if ( SDL_MUSTLOCK(screen) ) {
        if ( SDL_LockSurface(screen) < 0 ) {
            return;
        }
    }
    switch (screen->format->BytesPerPixel) {
        case 1: { /* Assuming 8-bpp */
            Uint8 *bufp;

            bufp = (Uint8 *)screen->pixels + y*screen->pitch + x;
            *bufp = color;
        }
        break;

        case 2: { /* Probably 15-bpp or 16-bpp */
            Uint16 *bufp;

            bufp = (Uint16 *)screen->pixels + y*screen->pitch/2 + x;
            *bufp = color;
        }
        break;

        case 3: { /* Slow 24-bpp mode, usually not used */
            Uint8 *bufp;

            bufp = (Uint8 *)screen->pixels + y*screen->pitch + x * 3;
            if(SDL_BYTEORDER == SDL_LIL_ENDIAN) {
                bufp[0] = color;
                bufp[1] = color >> 8;
                bufp[2] = color >> 16;
            } else {
                bufp[2] = color;
                bufp[1] = color >> 8;
                bufp[0] = color >> 16;
            }
        }
        break;

        case 4: { /* Probably 32-bpp */
            Uint32 *bufp;

            bufp = (Uint32 *)screen->pixels + y*screen->pitch/4 + x;
            *bufp = color;
        }
        break;
    }
    if ( SDL_MUSTLOCK(screen) ) {
        SDL_UnlockSurface(screen);
    }
}

int return_func(SDL_Surface *screen)
{
	SDL_Flip ((SDL_Surface *) screen);
	return 0;
}


int kpress_func(SDL_Surface *screen, SDLFont *font1, char *string, int x1, int y1)
{
	drawString ((SDL_Surface *) screen, font1, x1, y1, string);
	SDL_Flip ((SDL_Surface *) screen);
}

int pampass_conv( int num_msg, const struct pam_message **msg,
                         struct pam_response **resp, void *appdata_ptr )
{
int i;
struct pampass_data *namepassdata = (struct pampass_data *)appdata_ptr;
struct pam_response *reply;

if( !(reply = (struct pam_response *) calloc( num_msg, sizeof (struct pam_response) ) ) )
  return( PAM_CONV_ERR );

for( i=0; i<num_msg; i++ )
  {
  if( msg[i]->msg_style == PAM_PROMPT_ECHO_ON )
    {
    if( reply[i].resp = strdup( namepassdata->pd_username ) )
      reply[i].resp_retcode = PAM_SUCCESS;
    else
      reply[i].resp_retcode = PAM_CONV_ERR;
    }
  else if( msg[i]->msg_style == PAM_PROMPT_ECHO_OFF )
    {
    if( reply[i].resp = strdup( namepassdata->pd_password ) )
      reply[i].resp_retcode = PAM_SUCCESS;
    else
      reply[i].resp_retcode = PAM_CONV_ERR;
    }
  else if( (msg[i]->msg_style == PAM_ERROR_MSG) ||
           (msg[i]->msg_style == PAM_TEXT_INFO) )
    {
    reply[i].resp_retcode = PAM_SUCCESS;
    reply[i].resp = NULL;
    }
  else
    free( reply );
  }

*resp = reply;

return( PAM_SUCCESS );
}

/*int unixpass( char *service, char *username, char *password, char *crpass )
{
if( *crpass == '\0' )
  return(0);
return( strcmp( crypt( password, crpass ), crpass ) );
}*/

int doauth (char* username, char* passwd)
{
	pam_handle_t *pamh=NULL;
	struct pampass_data namepassdata;
	struct pam_conv conv;
	int retval;

	conv.conv = &pampass_conv;
	conv.appdata_ptr = &namepassdata;

	namepassdata.pd_username=username;
	namepassdata.pd_password=passwd;

	if ((retval = pam_start("xdm", username, &conv, &pamh)) != PAM_SUCCESS)
	{	
		printf ("PAM not initalized, giving up.\n");
		return (1);
	}

	
	
	if ((retval=pam_authenticate (pamh, 0)) != PAM_SUCCESS)
	{ 
		printf ("Bad username or password.\n");
		return (1);
	} else {
		printf ("Authenticated.\n");
	}

/*	if ((retval = pam_acct_mgmt(pamh, 0)) != PAM_SUCCESS) {
		printf ("Error.\n");
		exit (1);
	}*/

	if (pam_end(pamh,retval) != PAM_SUCCESS) {     /* close PAM */
		pamh = NULL;
		fprintf(stderr, "check_user: failed to release authenticator\n");
		return (1);
    	}

	return ( retval == PAM_SUCCESS ? 0:1 );	
}

int eventloop (void *screen)
{
  SDL_Event event;
  int kcount=0;
  int offset=0;
  int up=0;
  SDLFont *font1;
  char *string;
  char carret;
  char *carptr;
  char *str;
  char *username;
  char *passwd;
  char *astring;
  int x1, y1, x2, y2, font1_em, i;

  string=(char *) malloc (sizeof(char)*256);
  username=(char *) malloc (sizeof(char)*256);
  passwd=(char *) malloc (sizeof(char)*256);

  str=string;

  font1=initFont ("data/font1", 1, 0.4, 0.4, 1);
  font1_em=stringWidth(font1, "m");

  while (1)
  {
    while ( SDL_PollEvent(&event) )
    {
	if (up==0) { 
		x1=640/2;
		y1=480/2;
		x2=(640/2)+(font1_em*8);
		y2=(480/2)+30;
	} else {
		x1=(640/2);
		y1=(480/2)+40;
		x2=(640/2)+(font1_em*8);
		y2=(480/2)+70;
	}

      // If someone closes the prorgam, then quit
      if ( event.type == SDL_QUIT )  { exit (0);  }

      if ( event.type == SDL_KEYDOWN )
      {
        // If someone presses ESC, then quit
        if ( event.key.keysym.sym == SDLK_ESCAPE ) { exit (0); }
        if ( event.key.keysym.sym == SDLK_RETURN ) { 
		if (up==0) {
			memcpy (username, string, 256);
			up=1;
		} else {
			memcpy (passwd, string , 256);

			draw_rect ((SDL_Surface *) screen, 640/2, 480/2, (640/2)+(font1_em*8), (480/2)+30);
			draw_rect ((SDL_Surface *) screen, 640/2, 480/2+40, (640/2)+(font1_em*8), (480/2)+70);
			printf ("username: %s\npassword: %s\n", username, passwd);
			doauth(username, passwd);
			up=0;

		}

		str=string;
		kcount=0;
		memcpy (str, "\0", 1);
		SDL_Flip((SDL_Surface *) screen);

		continue ;
	}
	if ( event.key.keysym.sym == SDLK_BACKSPACE ) {
		if (kcount <= 0) continue;

		str--;
		kcount--;
		memcpy (str, "\0", 1);


		draw_rect ((SDL_Surface *) screen, x1, y1, x2, y2);

		if (up==0)
		{ 
			kpress_func ( (SDL_Surface *) screen, font1, string, x1, y1);	
		} else {
			astring=(char *) malloc ( sizeof(char)*256);
			for (i=0; i<kcount; i++)
			{
				memcpy (astring, "*", 1);
				astring++;
			}	
			astring-=kcount;
			kpress_func ( (SDL_Surface *) screen, font1, astring, x1, y1);	
		}
		SDL_Flip((SDL_Surface *) screen);
		continue ;
	}
	if ( event.key.keysym.sym == SDLK_RETURN ) { return_func((SDL_Surface *) screen); } else {
		if ( kcount >= 8 ) {
			fprintf (stderr, "No more space for char.\n");
			continue;
		}
		kcount++;

		switch ( event.key.keysym.sym ) {
		 	case SDLK_SPACE:		
				carret=' ';
				break;
			case SDLK_HASH:
				carret='#';
				break;
			case SDLK_DOLLAR:
				carret='$';
				break;
			case SDLK_ASTERISK:
			 	carret='*';	
				break;
			default:
				carptr=&carret;
				memcpy (carptr, SDL_GetKeyName (event.key.keysym.sym), 1);
		} 

		draw_rect ((SDL_Surface *) screen, x1, y1, x2, y2);

		memcpy (str, &carret, 1);
		str++;
		memcpy (str, "\0", 1);
//		kpress_func ( (SDL_Surface *) screen, font1, string, x1, y1);	
		if (up==0)
		{ 
			kpress_func ( (SDL_Surface *) screen, font1, string, x1, y1);	
		} else {
			astring=(char *) malloc ( sizeof(char)*256);
			for (i=0; i<kcount; i++)
			{
				memcpy (astring, "*\0", 2);
				astring++;
			}	
			astring-=kcount;
			kpress_func ( (SDL_Surface *) screen, font1, astring, x1, y1);	
		}

		continue;
	}
      }
    }
  }

  freeFont (font1);
}


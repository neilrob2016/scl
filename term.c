#include "globals.h"


/*** Switch to raw mode ***/
void rawOn()
{
	struct termios tio;

	if (isatty(STDIN))
	{
		tcgetattr(STDIN,&tio);
		tio.c_lflag &= ~ISIG;
		tio.c_iflag &= ~(IXON | ISTRIP);
		tcsetattr(STDIN,TCSANOW,&tio);
	}
}




/*** Switch keyboard echoing on ***/
void echoOn()
{
	struct termios tio;

	if (isatty(STDIN))
	{
		flags.echo = 1;
		tcgetattr(STDIN,&tio);
		tio.c_lflag |= ECHO;
		tcsetattr(STDIN,TCSANOW,&tio);
	}
}




/*** Switch echoing off ***/
void echoOff()
{
	struct termios tio;

	if (isatty(STDIN))
	{
		flags.echo = 0;
		tcgetattr(STDIN,&tio);
		tio.c_lflag &= ~ECHO;
		tcsetattr(STDIN,TCSANOW,&tio);
	}
}




/*** Switch canonical mode on ***/
void canonicalOn()
{
	struct termios tio;

	if (isatty(STDIN))
	{
		tcgetattr(STDIN,&tio);
		tio.c_lflag |= ICANON;
		tcsetattr(STDIN,TCSANOW,&tio);
	}
}





/*** Switch canonical mode off ***/
void canonicalOff()
{
	struct termios tio;

	if (isatty(STDIN)) 
	{
		tcgetattr(STDIN,&tio);
		tio.c_lflag &= ~ICANON;
		tio.c_cc[VTIME] = 0;
		tio.c_cc[VMIN] = 1;
		tcsetattr(STDIN,TCSANOW,&tio);
	}
}

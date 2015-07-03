/*******************************************************
 Windows HID simplification

 Alan Ott
 Signal 11 Software

 8/22/2009

 Copyright 2009
 
 This contents of this file may be used by anyone
 for any reason without any conditions and may be
 used as a starting point for your own applications
 which use HIDAPI.
********************************************************/

#include <stdio.h>
#include <wchar.h>
#include <string.h>
#include <stdlib.h>
#include "hidapi.h"
#include <time.h>

// Headers needed for sleeping.
#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

int sres, res, retry, silent = 0;
char serialstring[256];
unsigned char buf[256];
char printbuf[30000];
#define MAX_STR 255
hid_device *handle;
int i;
int lastresult = 0;


void
bCmd (const char *str)
{
  unsigned short int val = 0;
  sscanf (str, "%hd", &val);
  memset (buf, 0, sizeof (buf));
  buf[0] = 0;
  buf[1] = val;
  buf[2] = 0;
  res = hid_write (handle, buf, 0x41);
  if (res < 0)
    {
      printf ("Unable to write()\n");
      printf ("Error: %ls\n", hid_error (handle));
      exit (5);
    }
}


void
mCmd (char *str)
{

  lastresult = 0;

  memset (buf, 0, sizeof (buf));
  buf[0] = 0;
  buf[1] = 0x60;
  buf[2] = 0;

  sprintf ((char *) &buf[3], "%s\r\n", str);
  buf[2] = strlen ((char *) &buf[3]);

  res = hid_write (handle, buf, 0x41);
  if (res < 0)
    {
      printf ("Unable write()\n");
      printf ("Error: %ls\n", hid_error (handle));
    }
}

void
gReply (int timeout = 3, int showresult = 1)
{
  timeout = timeout * 100;

  int dpoint = 0;
  int nodatamax = timeout;
  memset (buf, 0, sizeof (buf));
  int nodata = 0;
  while (nodata < nodatamax)
    {
      res = 0;

      while ((res == 0) && (nodata < nodatamax))
	{
	  memset (buf, 0, sizeof (buf));
	  res = hid_read (handle, buf, sizeof (buf));
	  if (res == 0)
	    {
	      nodata++;
	      usleep (1 * 500);
	    }
	}
      if (res < 0)
	{
	  printf ("Unable to read()\n");
	  nodata++;
	  exit (5);
	}
      if (res > 0)
	{
	  nodata = 0;

	  if (res != 64)
	    printf ("\nRead bytes %d:\n\n", res);

	  if (buf[0] == 0x60)
	    {
	      int slen = buf[1];


	      if ((dpoint + slen) > sizeof (printbuf))
		{
		  if (showresult)
		    printf ("%s", printbuf);
		  dpoint = 0;
		}

	      bcopy (&buf[2], &printbuf[dpoint], slen);
	      dpoint = dpoint + slen;
	      printbuf[dpoint] = 0x0;

	      if (dpoint > 7)
		{
		  if (strncmp
		      ("CMD OK\r\n", &printbuf[dpoint - 8],
		       sizeof (printbuf) - dpoint) == 0)
		    {
		      lastresult = 1;

		      break;
		    }
		  if (strncmp
		      ("CMD Fail!\r\n", &printbuf[dpoint - 11],
		       sizeof (printbuf) - dpoint) == 0)
		    {
		      lastresult = -1;
		      break;
		    }
		}


	    }
	  else
	    {
	    }
	}

    }
  if (showresult)
    {
      if ((lastresult == 1))
	printf ("\n%s\n", printbuf);
    }

}


void
mCmdMacro (char *str, int showresult = 1)
{

  int failcounter = 0;
  lastresult = -5;
  while ((lastresult != 1) && (failcounter < 3))
    {

      if (failcounter > 0)
	{
	  printf ("Fail counter: %d,%s\n", failcounter, str);
	  sleep (1);
	  gReply (3, 0);
	}

      mCmd (str);
      gReply (3, showresult);
      failcounter++;
    }

  if (lastresult != 1)
    {
      printf ("Last result: %d\n", lastresult);
      exit (5);
    }
}


void
cmode (int value)
{
  FILE *fp = fopen ("/sys/devices/platform/mt_usb/cmode", "w");
  if (!fp)
    return;
  fprintf (fp, "%d\n", value);
  fclose (fp);
  if (value == 1)
    {
      sleep (3);
    }
}


int
main (int argc, char *argv[])
{

#ifdef WIN32
  UNREFERENCED_PARAMETER (argc);
  UNREFERENCED_PARAMETER (argv);
#endif

  if ((argc > 1) && (argv[1][0] == 'S'))
    silent = 1;

  struct tm t;
  time_t t_of_day, bg_t_of_day;
  int unknown1, unknown2, unknown3, unknown4, unknown5, unknown6, unknown7,
    unknown8;
  int mgdl, recordtype, recordnumber;


  cmode (1);

  if (hid_init ())
    return -1;



  // Open the device using the VID, PID,
  // and optionally the Serial number.

  retry = 12;

  while ((!handle) && (retry > 0))
    {

      handle = hid_open (0x1a61, 0x3460, NULL);

      if (!handle)
	{
	  if (!silent)
	    printf ("Sleeping\n");
	  sleep (1);
	  retry = retry - 1;
	}

    }

  if (!handle)
    {
      printf ("Unable to open\n");
      return 1;
    }

  hid_set_nonblocking (handle, 1);
  res = hid_read (handle, buf, 64);


  // Set the hid_read() function to be non-blocking.

  gReply (1, 0);

  bCmd ("4");
  gReply (3, 0);
  bCmd ("5");
  gReply (3, 0);
  bCmd ("21");
  gReply (3, 0);
  bCmd ("1");
  gReply (3, 0);

  mCmdMacro ("$serlnum?", 0);

  sres = sscanf (printbuf, "%s", serialstring);

  if (sres != 1)
    {
      exit (5);
    }

  mCmdMacro ("$date?", 0);

  sres = sscanf (printbuf, "%d,%d,%d", &t.tm_mon, &t.tm_mday, &t.tm_year);

  if (sres != 3)
    {
      exit (5);
    }

  t.tm_year = t.tm_year + 2000;

  mCmdMacro ("$time?", 0);
  sres = sscanf (printbuf, "%d,%d", &t.tm_hour, &t.tm_min);

  if (sres != 2)
    {
      exit (5);
    }

  t.tm_sec = 0;
  t.tm_isdst = -1;
  t.tm_year = t.tm_year - 1900;
  t.tm_mon = t.tm_mon - 1;

  t_of_day = mktime (&t);

  mCmdMacro ("$result?", 0);
  sres =
    sscanf (printbuf, "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d",
	    &recordtype, &recordnumber, &t.tm_mon, &t.tm_mday, &t.tm_year,
	    &t.tm_hour, &t.tm_min, &unknown1, &unknown2, &unknown3, &unknown4,
	    &unknown5, &unknown6, &mgdl, &unknown7, &unknown8);

  if (sres != 16)
    {
      exit (5);
    }

  if (recordtype != 0)
    {
      exit (5);
    }


  t.tm_sec = 0;
  t.tm_isdst = -1;
  t.tm_year = t.tm_year + 2000;
  t.tm_year = t.tm_year - 1900;
  t.tm_mon = t.tm_mon - 1;
  bg_t_of_day = mktime (&t);
  long int timediff = t_of_day - bg_t_of_day;

  float mmol = mgdl / 18.018;
  printf
    ("Serial: %s :: Time since last bg: %d secs :: %.1f mmol/l / %d mg/dl\n",
     serialstring, timediff, mmol, mgdl);

  hid_close (handle);
  /* Free static HIDAPI objects. */
  hid_exit ();

#ifdef WIN32
  system ("pause");
#endif

  cmode (0);

  return 0;
}

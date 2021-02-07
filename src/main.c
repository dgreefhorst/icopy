/**
 * Interactive N copy for Atari Fujinet
 * Currently only supports copying from N: to D: devices
 *
 * Author: Danny Greefhorst, based on sources by Thomas Cherryhomes
 *  <dgreefhorst@archixl.nl>
 *
 * Released under GPL 3.0
 * See COPYING for details.
 */

#include <atari.h>
#include <string.h>
#include <stdlib.h>
#include <peekpoke.h>
#include <stdbool.h>
#include <conio.h>
#include "sio.h"
#include "conio.h"
#include "err.h"
#include "blockio.h"

// From ncopy
#define D_DEVICE_DATA      1
unsigned char yvar;
unsigned char sourceDeviceSpec[255];
unsigned char destDeviceSpec[255];
unsigned char data[16384]; 
unsigned short data_len;
unsigned short block_len;
unsigned char* dp;
char errnum[4];
char* pToken;
char* pWildcardStar, *pWildcardChar;
unsigned char sourceUnit=1, destUnit=1;

// New code
unsigned char buf[256];
unsigned char u;
unsigned char x;
unsigned char y;
unsigned char j;
unsigned char lastlen;
unsigned short ab=0;
unsigned char directorybuf[256];
bool morefiles;
unsigned char directory[22][32];

void nopen(unsigned char unit, char* buf, unsigned char aux1, unsigned char aux2)
{
  OS.dcb.ddevic=0x71;
  OS.dcb.dunit=unit;
  OS.dcb.dcomnd='O';
  OS.dcb.dstats=0x80;
  OS.dcb.dbuf=buf;
  OS.dcb.dtimlo=0x1f;
  OS.dcb.dbyt=256;
  OS.dcb.daux1=aux1;
  OS.dcb.daux2=aux2; // Long dir.
  siov();

  if (OS.dcb.dstats!=1)
    {
      err_sio();
      exit(OS.dcb.dstats);
    }
}

void nclose(unsigned char unit)
{
  OS.dcb.ddevic=0x71;
  OS.dcb.dunit=unit;
  OS.dcb.dcomnd='C';
  OS.dcb.dstats=0x00;
  OS.dcb.dbuf=NULL;
  OS.dcb.dtimlo=0x1f;
  OS.dcb.dbyt=0;
  OS.dcb.daux1=0;
  OS.dcb.daux2=0;
  siov();

  if (OS.dcb.dstats!=1)
    {
      err_sio();
      exit(OS.dcb.dstats);
    }
}

void nread(unsigned char unit, char* buf, unsigned short len)
{
  OS.dcb.ddevic=0x71;
  OS.dcb.dunit=unit;
  OS.dcb.dcomnd='R';
  OS.dcb.dstats=0x40;
  OS.dcb.dbuf=buf;
  OS.dcb.dtimlo=0x1f;
  OS.dcb.dbyt=len;
  OS.dcb.daux=len;
  siov();

  if (OS.dcb.dstats!=1)
    {
      err_sio();
      exit(OS.dcb.dstats);
    }
}

void nwrite(unsigned char unit, char* buf, unsigned short len)
{
  OS.dcb.ddevic=0x71;
  OS.dcb.dunit=unit;
  OS.dcb.dcomnd='W';
  OS.dcb.dstats=0x80;
  OS.dcb.dbuf=buf;
  OS.dcb.dtimlo=0x1f;
  OS.dcb.dbyt=len;
  OS.dcb.daux=len;
  siov();
}

void nstatus(unsigned char unit)
{
  OS.dcb.ddevic=0x71;
  OS.dcb.dunit=unit;
  OS.dcb.dcomnd='S';
  OS.dcb.dstats=0x40;
  OS.dcb.dbuf=&OS.dvstat;
  OS.dcb.dtimlo=0x1f;
  OS.dcb.dbyt=4;
  OS.dcb.daux1=0;
  OS.dcb.daux2=0;
  siov();

  if (OS.dcb.dstats!=1)
    {
      err_sio();
      exit(OS.dcb.dstats);
    }
}


// From ncopy (with slight modifications)

void print_error(void)
{
  print("ERROR- ");
  itoa(yvar,errnum,10);
  print(errnum);
  print("\x9b");
}

bool detect_wildcard(char* buf)
{
  pWildcardStar=strchr(buf, '*');
  pWildcardChar=strchr(buf, '?');
  return ((pWildcardStar!=NULL) || (pWildcardChar!=NULL));
}

bool parse_filespec()
{
  // Put EOLs on the end.
  sourceDeviceSpec[strlen(sourceDeviceSpec)]=0x9B;
  destDeviceSpec[strlen(destDeviceSpec)]=0x9B;

  // Check for valid device name chars
  if (sourceDeviceSpec[0]<0x41 || sourceDeviceSpec[0]>0x5A)
    return false;
  else if (destDeviceSpec[0]<0x41 || destDeviceSpec[0]>0x5A)
    return false;

  // Check for proper colon placement
  if (sourceDeviceSpec[1]!=':' && sourceDeviceSpec[2]!=':')
    return false;
  else if (destDeviceSpec[1]!=':' && destDeviceSpec[2]!=':')
    return false;

  // Try to assign unit numbers.
  if (sourceDeviceSpec[1] != ':')
    sourceUnit=sourceDeviceSpec[1]-0x30;
  if (destDeviceSpec[1] != ':')
    destUnit=destDeviceSpec[1]-0x30;

  return true;
}

int _copy_n_to_d(void)
{
  nopen(sourceUnit,sourceDeviceSpec,4,0);

  if (OS.dcb.dstats!=1)
    {
      nstatus(destUnit);
      yvar=OS.dvstat[3];
      print_error();
      nclose(destUnit);
    }

  open(D_DEVICE_DATA,8,destDeviceSpec,strlen(destDeviceSpec));

  if (yvar!=1)
    {
      print_error();
      close(D_DEVICE_DATA);
      return yvar;
    }  

  do
    {
      nstatus(sourceUnit);
      data_len=OS.dvstat[1]*256+OS.dvstat[0];

      if (data_len==0)
	break;
      
      // Be sure not to overflow buffer!
      if (data_len>sizeof(data))
	data_len=sizeof(data);

      nread(sourceUnit,data,data_len); // add err chk

      put(D_DEVICE_DATA,data,data_len);
      
    } while (data_len>0);

  nclose(sourceUnit);
  close(D_DEVICE_DATA);
  return 0;
}

int copy_n_to_d(void)
{
  if (detect_wildcard(sourceDeviceSpec)==false)
    return _copy_n_to_d();
}

bool valid_cio_device(char d)
{
  return (d!='N' && (d>0x40 && d<0x5B));
}

bool valid_network_device(char d)
{
  return (d=='N');
}

// New code

int ncopy()
{
	if (parse_filespec()==0)
	{
		print("COULD NOT PARSE FILESPEC.\x9b");
      return(1);
    }

	if (destDeviceSpec[0]!='D') {
		print("ONLY D: DEVICES SUPPORTED AS TARGET\x9b");
		return(1);
	}
	
	print(sourceDeviceSpec);
	print(destDeviceSpec);

    return copy_n_to_d();  
}

unsigned char getnextbyte(unsigned char u) {
	unsigned char result;
	if (j == ab) {
		nstatus(u);
		ab=OS.dvstat[1]*256+OS.dvstat[0];
		if (ab>255) 
			ab=255;
		else if (ab==0) {
			morefiles=false;
			return 0;
		}
		memset(&directorybuf,0,256);
		nread(u,directorybuf,ab);
		j=0;
	} 
	result = directorybuf[j];
	j++;
	return result;
}

void getfilename(unsigned char u, unsigned char x) {
	unsigned char h=0;
	unsigned char b;
	unsigned lastnonblank=0;
	do {
		b = getnextbyte(u);
		if (!morefiles) return;
		directory[x][h]=b;
		if (b!=32) 
			lastnonblank = h;
		h++;
	} while (h<30);
	directory[x][lastnonblank+1]=0;
	do {
		b = getnextbyte(u);
		if (!morefiles) return;
		h++;
	} while (b!=0x9b);
}

bool selectfile_fromn() {
	u=1;
	x=0;
	y=0;
	j=0;
  
	// if no device, set a device path.
	if ((buf[1]!=':') && (buf[2]!=':')) {
		memmove(&buf[2],&buf[0],sizeof(buf)-3);
		buf[0]='N';
		buf[1]=':';
		buf[2]=0;
	} 
	else if (buf[2]==':') {
		u=buf[1]-0x30;
		buf[3]=0;
		if (!valid_network_device(buf[0])) {
			print("ONLY N: DEVICES SUPPORTED AS SOURCE\x9b");
			return false;
		}
	}

	nopen(u,buf,6,128);
	morefiles=true;

	do {
		getfilename(u,x);
		// print filename if it is not the last name, which only shows the number of sectors
		if (morefiles) { 	
			cputc(x+65+128);
			cputc(32);
			print(directory[x]);
			print("\n");
		}
		// ask selection when 22 lines are shown on screen or the end of the file list has been reached 
		if (x==21 || !morefiles) {
			if (x==0) {
				print("NO FILES FOUND\x9b");
				y = cgetc();
				return false;
			}				
			print("SELECT FILE OR DIRECTORY OR \xC5\xD3\xC3\x9b");
			y = cgetc();
			if ((y>96) && (y<119) && (y-97 < x)) {
				strcat(buf,directory[y-97]);
				nclose(u);
				if (buf[strlen(buf)-1]=='/') {
					return selectfile_fromn();
				}
				else
					return true;
			} else 
				if (y==27) return false;
			x = 0;
		} else 
			++x;
	} while(morefiles);

	nclose(u);
	return false;
}

void main() {
	OS.lmargn=2;
	print("ENTER SOURCE PATH OR \xD2\xC5\xD4\xD5\xD2\xCE for N:");
	get_line(buf,240);
	if (selectfile_fromn()) {
		strcpy(sourceDeviceSpec,buf);
		print("\x9b");
		print("ENTER TARGET PATH WITHOUT WILDCARDS\x9b");
		get_line(destDeviceSpec,240);	
		ncopy();
	} 
}
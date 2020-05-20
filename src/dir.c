/* -*- c-basic-offset: 2; tab-width: 2; indent-tabs-mode: nil -*-
 * vi: set shiftwidth=2 tabstop=2 expandtab:
 * :indentSize=2:tabSize=2:noTabs=true:
 */
/*
 * dir.c
 *
 *  Created on: 10.01.2009
 *      Author: bader
 *
 * DraCopy (dc*) is a simple copy program.
 * DraBrowse (db*) is a simple file browser.
 *
 * Since both programs make use of kernal routines they shall
 * be able to work with most file oriented IEC devices.
 *
 * Created 2009 by Sascha Bader
 *
 * The code can be used freely as long as you retain
 * a notice describing original source and author.
 *
 * THE PROGRAMS ARE DISTRIBUTED IN THE HOPE THAT THEY WILL BE USEFUL,
 * BUT WITHOUT ANY WARRANTY. USE THEM AT YOUR OWN RISK!
 *
 * Newer versions might be available here: http://www.sascha-bader.de/html/code.html
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <conio.h>
#include <cbm.h>
#include "dir.h"
#include "defines.h"

# define CBM_T_FREE 100

/**
 * read directory of device @p device and print information to window @p context.
 * @param[in,out] dir pointer to Directory object, if dir!=NULL it will be freed.
 * @param device CBM device number
 * @param context window ID, must be 0 or 1.
 * @return new allocated Directory object.
 */
Directory * readDir(Directory  * dir, BYTE device, BYTE context)
{
	DirElement * previous = NULL;
	DirElement * current = NULL;

  BYTE xpos = 0xff;

	unsigned char stat = 0;

	// free old dir
	if (dir!=NULL)
    {
      freeDir(&dir);
      dir=NULL;
    }

	if (cbm_opendir(8, device) != 0)
    {
      cputs("could not open directory");
      //printErrorChannel(device);
      //cgetc();
      cbm_closedir(8);
      return NULL;
    }

	do
    {
      DirElement * current = (DirElement *) malloc(sizeof(DirElement));
      memset (current,0,sizeof(DirElement));

      stat=myCbmReadDir(8, &(current->dirent));
      if (stat==0)
        {
          // print progress bar
          if (xpos >= MENUX)
            {
              revers(0);
              gotoxy((context==0) ? DIR1X : DIR2X, (context==0) ? DIR1Y : DIR2Y );
              cputs("                         ");
              gotoxy((context==0) ? DIR1X : DIR2X, (context==0) ? DIR1Y : DIR2Y );
              revers(1);
              xpos = 0;
            }
          else
            {
              cputc(' ');
              ++xpos;
            }

          if (dir==NULL)
            {
              // initialize directory
              dir = (Directory *) malloc(sizeof(Directory));
              memcpy(dir->name, current->dirent.name, 16);
              dir->name[16] = ',';
              dir->name[17] = current->dirent.type;
              dir->name[18] = current->dirent.access;
              dir->name[19] = 0;
              dir->firstelement=NULL;
              dir->selected=NULL;
              dir->flags = 0;
              dir->pos=0;
              dir->free = 0;
              free(current);
            }
          else if (current->dirent.type==CBM_T_FREE)
            {
              // blocks free entry
              dir->free=current->dirent.size;
              free(current);
            }
          else if (dir->firstelement==NULL)
            {
              // first element
              dir->firstelement = current;
              dir->selected = current;
              previous=current;
            }
          else
            {
              // all other elements
              current->previous=previous;
              previous->next=current;
              previous=current;
            }

          //cprintf("file:%s\n",current->dirent.name);
        }
    }
	while(stat==0);

	cbm_closedir(8);

	revers(0);

	return dir;
}

/**
 * @return 0 upon success, @p l_dirent was set.
 * @return >0 upon error.
 */
unsigned char
myCbmReadDir (unsigned char device, struct cbm_dirent* l_dirent)
{
  unsigned char byte, i, byte2, byte3;

  if (cbm_k_chkin (device) != 0)
    {
      cbm_k_clrch ();
      return 1;
    }

  if (cbm_k_readst () == 0) {
    l_dirent->access = 0;

    // skip next basic line: 0x01, 0x01
    cbm_k_basin ();
    cbm_k_basin ();

    // read file size
    l_dirent->size = cbm_k_basin();
    l_dirent->size |= (cbm_k_basin()) << 8;

    i = 0;
    byte = cbm_k_basin();

    // handle "B" BLOCKS FREE
    if (byte == 'b') {
      l_dirent->type = CBM_T_FREE;
      l_dirent->name[i++] = byte;
      while ((byte = cbm_k_basin ()) != '\"'  )
        {
          if (cbm_k_readst () != 0)
            {     /* ### Included to prevent */
              cbm_k_clrch ();             /* ### Endless loop */
              l_dirent->name[i] = '\0';

              return 0;                   /* ### Should be probably removed */
            }
          if (i < sizeof (l_dirent->name) - 1) {
            l_dirent->name[i] = byte;
            ++i;
          }
        }
      l_dirent->name[i] = '\0';
      return 0;
    }

    // read file name
    if (byte != '\"')
      {
        while (cbm_k_basin() != '\"') {
          if (cbm_k_readst () != 0) {   /* ### Included to prevent */
            cbm_k_clrch ();           /* ### Endless loop */
            return 3;                 /* ### Should be probably removed */
          }                             /* ### */
        }
      }

    while ((byte = cbm_k_basin ()) != '\"'  )
      {
        if (cbm_k_readst () != 0) {     /* ### Included to prevent */
          cbm_k_clrch ();             /* ### Endless loop */
          return 4;                   /* ### Should be probably removed */
        }

        /* ### */

        if (i < sizeof (l_dirent->name) - 1) {
          l_dirent->name[i] = byte;
          ++i;
        }
      }
    l_dirent->name[i] = '\0';

    // read file type
    while ((byte=cbm_k_basin ()) == ' ') {
      if (cbm_k_readst ()) {          /* ### Included to prevent */
        cbm_k_clrch ();             /* ### Endless loop */
        return 5;                   /* ### Should be probably removed */
      }                               /* ### */
    }

    byte2 = cbm_k_basin();
    byte3 = cbm_k_basin();

#define X(a,b,c) byte==a && byte2==b && byte3==c

#if 1
    if (X('p','r','g'))
      {
        l_dirent->type = CBM_T_PRG;
      }
    else if (X('s','e','q'))
      {
        l_dirent->type = CBM_T_SEQ;
      }
    else if (X('u','s','r'))
      {
        l_dirent->type = CBM_T_USR;
      }
    else if (X('r','e','l'))
      {
        l_dirent->type = CBM_T_REL;
      }
    else if (X('c','b','m'))
      {
        l_dirent->type = CBM_T_CBM;
      }
    else if (X('d','i','r'))
      {
        l_dirent->type = CBM_T_DIR;
      }
    else if (X('v','r','p'))
      {
        l_dirent->type = CBM_T_VRP;
      }
    else if (byte3 = ' ')
      {
        // reading the disk name line
        l_dirent->type = byte;
        l_dirent->access = byte2;

        while (cbm_k_basin() != 0) {
          if (cbm_k_readst () != 0) { /* ### Included to prevent */
            cbm_k_clrch ();         /* ### Endless loop */
            return 8;               /* ### Should be probably removed */
          }                           /* ### */
        }

        cbm_k_clrch ();
        return 0;                         /* Line read successfuly */
      }
    else
      {
        l_dirent->type = CBM_T_OTHER;
      }
#else
    switch (byte) {
    case 's':
      l_dirent->type = CBM_T_SEQ;
      break;
    case 'p':
      l_dirent->type = CBM_T_PRG;
      break;
    case 'u':
      l_dirent->type = CBM_T_USR;
      break;
    case 'r':
      l_dirent->type = CBM_T_REL;
      break;
    case 'c':
      l_dirent->type = CBM_T_CBM;
      break;
    case 'd':
      l_dirent->type = CBM_T_DIR;
      break;
    case 'v':
      l_dirent->type = CBM_T_VRP;
      break;
    default:
      l_dirent->type = CBM_T_OTHER;
    }
#endif

    // read access
    byte = cbm_k_basin ();

    l_dirent->access = (byte == 0x3C) ? CBM_A_RO : CBM_A_RW;

    if (byte != 0) {
      while (cbm_k_basin() != 0) {
        if (cbm_k_readst () != 0) { /* ### Included to prevent */
          cbm_k_clrch ();         /* ### Endless loop */
          return 6;               /* ### Should be probably removed */
        }                           /* ### */
      }
    }

    cbm_k_clrch ();
    return 0;                         /* Line read successfuly */
  }

  return 7;
}

/*
 * free memory of directory structure
 */
void freeDir(Directory * * dir)
{
  DirElement * next;
	DirElement * acurrent;
	if (*dir!=NULL)
    {
      acurrent = (*dir)->firstelement;

      while (acurrent!=NULL)
        {
          next = acurrent->next;
          free(acurrent);
          acurrent=next;
        }

      free(*dir);
    }
	*dir=NULL;
}


/*
 * Remove an entry from its data structure (directory)
 */
void removeFromDir(DirElement * current)
{
 	if (current!=NULL)
    {
      if (current->previous!=NULL)
        {
          current->previous->next = current->next;
        }

      if (current->next!=NULL)
        {
          current->next->previous = current->previous;
        }
	    free(current);
    }
}

/*********************************************************************
 * 
 * dbread_debug.c - Driver to read a dataset which is intended to debug
 *                  lineread.
 *
 * Copyright (C) 2006 Patricio Rojo
 * 
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General 
 * Public License as published by the Free Software Foundation.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 *
 **********************************************************************/

#include <lineread.h>


FILE *fp = NULL;
_Bool partitionread=0;

static _Bool
db_find(const char *name)
{
  int len = strlen(name);
  if (len >= 7 && strncmp("debugDB",name+len-7,7)==0){
    struct stat st;
    stat (name, &st);
    if (S_ISREG(st.st_mode) && st.st_size > 0)
      return 1;
  }

  return 0;
}


/********************************************************/


static int
db_open(char *dbname, 
	char *dbaux)
{
  if((fp = fopen(dbname,"r")) == NULL)
    mperror(MSGP_USER,
	    "Could not open file '%s' for reading\n"
	    , dbname);

  return LR_OK;
}


/********************************************************/


static int
db_close()
{
  if(fp)
    fclose(fp);

  return LR_OK;
}


/********************************************************/


/* \fcnfh
   Reads transition info from debug DB, format is:
   --
   lambda1 isoid1 elow1 gf1
   lambda2 isoid2 elow2 gf2
   --

   @returns -1 if db_part has not been called before
*/
static long int
db_info(struct linedb **lineinfo,
	double wav1,
	double wav2)
{
  if (!partitionread)
    return -1;

  int maxline = 200;
  char line[maxline], *lp;

  long int i=0, alloc=8;
  *lineinfo = (struct linedb *)calloc(alloc, 
				      sizeof(struct linedb));

  while((lp = fgets(line, maxline, fp)) != NULL){
    double wav = strtod(lp, &lp);
    if (wav < wav1) continue;
    if (wav > wav2) break;

    if (i > alloc) 
      *lineinfo = (struct linedb *)realloc(*lineinfo, 
					   (alloc<<=1)*sizeof(struct linedb));

    (*lineinfo)[i].wl   = wav;
    (*lineinfo)[i].isoid   = strtol(lp, &lp, 10);
    (*lineinfo)[i].elow = strtod(lp, &lp);
    (*lineinfo)[i++].gf   = strtod(lp, &lp);
  }
  
  return i;
}


/********************************************************/

/* \fcnfh
   Reads partition info from debug DB, format is:
   --
   name
   nt t(1) t(2) t(3) .. t(nt)
   niso m(1)i(1) m(2)i(2) .. m(niso)i(niso)
   Z(1,1) Z(1,2) .. Z(1,niso)
   Z(2,1) Z(2,2) .. Z(2,niso)
   ..
   Z(nt,1) Z(nt,2) .. Z(nt, niso)
   CS(1,1) CS(1,2) .. CS(1,niso)
   CS(2,1) CS(2,2) .. CS(2,niso)
   ..
   CS(nt,1) CS(nt,2) .. CS(nt, niso)
   --
*/
static int
db_part(char **name,
	unsigned short *nT,
	PREC_TEMP **T,
	unsigned short *niso,
	char ***isonames,
	PREC_MASS **mass,
	PREC_Z ***Z,
	PREC_CS ***CS)
{
  int maxlen=200;

  char line[maxlen], *lp;

  if(! (*name = strdup(fgets(line, maxlen, fp))) )
    mperror(MSGP_USER, "Invalid debug DB name\n");
  (*name)[strlen(*name)-1] = '\0';

  if(! (fgets(line, maxlen, fp)) )
    mperror(MSGP_USER, "Invalid debug DB temp\n");
  *nT = strtol(line, &lp, 10);
  *T = (PREC_TEMP *)calloc(*nT, sizeof(PREC_TEMP));
  for (int i=0 ; i<*nT ; i++)
    (*T)[i] = strtod(lp, &lp);

  if(! (fgets(line, maxlen, fp)) )
    mperror(MSGP_USER, "Invalid debug DB iso\n");
  *niso = strtol(line, &lp, 10);
  *mass = (PREC_MASS *)calloc(*niso, sizeof(PREC_MASS));
  *isonames = (char **)calloc(*niso, sizeof(char *));
  for (int i=0 ; i<*niso ; i++){
    (*mass)[i] = strtod(lp, &lp);
    lp[index(lp, ' ')-lp] = '\0';
    (*isonames)[i] = strdup(lp);
    lp += strlen(lp) + 1;
  }

  *Z   = (PREC_Z  **)calloc(*niso,     sizeof(PREC_Z *) );
  *CS  = (PREC_CS **)calloc(*niso,     sizeof(PREC_CS *));
  **Z  = (PREC_Z   *)calloc(*niso**nT, sizeof(PREC_Z)   );
  **CS = (PREC_CS  *)calloc(*niso**nT, sizeof(PREC_CS)  );

  for(int i=0 ; i<*niso ; i++){
    if(! (fgets(line, maxlen, fp)) )
      mperror(MSGP_USER, "Invalid debug DB Z\n");
    (*Z)[i]  = **Z  + *nT*i;
    for(int j=0 ; j<*nT ; j++)
      (*Z)[i][j]  = strtod(lp, &lp);
  }
  for(int i=0 ; i<*niso ; i++){
    if(! (fgets(line, maxlen, fp)) )
      mperror(MSGP_USER, "Invalid debug DB CS\n");
    (*CS)[i] = **CS + *nT*i;
    for(int j=0 ; j<*nT ; j++)
      (*CS)[i][j] = strtod(lp, &lp);
  }

  partitionread = 1;
  return LR_OK;
}


static const driver_func pdriverf_debug = {
  "DEBUGGING driver",
  &db_find,
  &db_open,
  &db_close,
  &db_info,
  &db_part
};

driver_func *
initdb_debug()
{
  return (driver_func *)&pdriverf_debug;
}

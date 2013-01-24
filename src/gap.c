/****************************************************************************
**
*W  gap.c                       GAP source                       Frank Celler
*W                                                         & Martin Schönert
**
**
*Y  Copyright (C)  1996,  Lehrstuhl D für Mathematik,  RWTH Aachen,  Germany
*Y  (C) 1998 School Math and Comp. Sci., University of St Andrews, Scotland
*Y  Copyright (C) 2002 The GAP Group
**
**  This file contains the various read-eval-print loops and  related  stuff.
*/
#include        <stdio.h>
#include        <assert.h>
#include        <string.h>              /* memcpy */
#include        <stdlib.h>

#include        "system.h"              /* system dependent part           */

#ifdef HAVE_SYS_STAT_H
#include        <sys/stat.h>
#endif

/* TL: extern char * In; */

#include        "gasman.h"              /* garbage collector               */
#include        "objects.h"             /* objects                         */
#include        "scanner.h"             /* scanner                         */

#define INCLUDE_DECLARATION_PART
#include        "gap.h"                 /* error handling, initialisation  */
#undef  INCLUDE_DECLARATION_PART

#include        "read.h"                /* reader                          */

#include        "gvars.h"               /* global variables                */
#include        "calls.h"               /* generic call mechanism          */
#include        "opers.h"               /* generic operations              */

#include        "ariths.h"              /* basic arithmetic                */

#include        "integer.h"             /* integers                        */
#include        "rational.h"            /* rationals                       */
#include        "cyclotom.h"            /* cyclotomics                     */
#include        "finfield.h"            /* finite fields and ff elements   */

#include        "bool.h"                /* booleans                        */
#include        "macfloat.h"            /* machine doubles                 */
#include        "permutat.h"            /* permutations                    */

#include        "records.h"             /* generic records                 */
#include        "precord.h"             /* plain records                   */

#include        "lists.h"               /* generic lists                   */
#include        "listoper.h"            /* operations for generic lists    */
#include        "listfunc.h"            /* functions for generic lists     */
#include        "plist.h"               /* plain lists                     */
#include        "set.h"                 /* plain sets                      */
#include        "vector.h"              /* functions for plain vectors     */
#include        "vecffe.h"              /* functions for fin field vectors */
#include        "blister.h"             /* boolean lists                   */
#include        "range.h"               /* ranges                          */
#include        "string.h"              /* strings                         */
#include        "vecgf2.h"              /* functions for GF2 vectors       */
#include        "vec8bit.h"             /* functions for other compressed
                                           GF(q) vectors                   */

#include        "objfgelm.h"            /* objects of free groups          */
#include        "objpcgel.h"            /* objects of polycyclic groups    */
#include        "objscoll.h"            /* single collector                */
#include        "objccoll.h"            /* combinatorial collector         */
#include        "objcftl.h"             /* from the left collect           */

#include        "dt.h"                  /* deep thought                    */
#include        "dteval.h"              /* deep though evaluation          */

#include        "sctable.h"             /* structure constant table        */
#include        "costab.h"              /* coset table                     */
#include        "tietze.h"              /* tietze helper functions         */

#include        "code.h"                /* coder                           */

#include        "exprs.h"               /* expressions                     */
#include        "stats.h"               /* statements                      */
#include        "funcs.h"               /* functions                       */


#include        "intrprtr.h"            /* interpreter                     */

#include        "compiler.h"            /* compiler                        */

#include        "compstat.h"            /* statically linked modules       */

#include        "saveload.h"            /* saving and loading              */

#include        "streams.h"             /* streams package                 */
#include        "sysfiles.h"            /* file input/output               */
#include        "weakptr.h"             /* weak pointers                   */

#ifdef GAPMPI
#include        "gapmpi.h"              /* ParGAP/MPI                      */
#endif

#ifdef WITH_ZMQ
#include	"zmqgap.h"		/* GAP ZMQ support		   */
#endif

#include        "thread.h"
#include        "tls.h"
#include        "threadapi.h"
#include        "aobjects.h"
#include        "objset.h"

#include        "vars.h"                /* variables                       */

#include        "intfuncs.h"
#include        "iostream.h"

/****************************************************************************
**

*V  Last  . . . . . . . . . . . . . . . . . . . . . . global variable  'last'
**
**  'Last',  'Last2', and 'Last3'  are the  global variables 'last', 'last2',
**  and  'last3', which are automatically  assigned  the result values in the
**  main read-eval-print loop.
*/
UInt Last;


/****************************************************************************
**
*V  Last2 . . . . . . . . . . . . . . . . . . . . . . global variable 'last2'
*/
UInt Last2;


/****************************************************************************
**
*V  Last3 . . . . . . . . . . . . . . . . . . . . . . global variable 'last3'
*/
UInt Last3;


/****************************************************************************
**
*V  Time  . . . . . . . . . . . . . . . . . . . . . . global variable  'time'
**
**  'Time' is the global variable 'time', which is automatically assigned the
**  time the last command took.
*/
UInt Time;


/****************************************************************************
**
*F  ViewObjHandler  . . . . . . . . . handler to view object and catch errors
**
**  This is the function actually called in Read-Eval-View loops.
**  We might be in trouble if the library has not (yet) loaded and so ViewObj
**  is not yet defined, or the fallback methods not yet installed. To avoid
**  this problem, we check, and use PrintObj if there is a problem
**
**  We also install a hook to use the GAP level function 'CustomView' if
**  it exists. This can for example be used to restrict the amount of output
**  or to show long output in a pager or .....
**  
**  This function also supplies the \n after viewing.
*/
UInt ViewObjGVar;
UInt CustomViewGVar;

void ViewObjHandler ( Obj obj )
{
  volatile Obj        func;
  volatile Obj        cfunc;
  syJmp_buf             readJmpError;

  /* get the functions                                                   */
  func = ValAutoGVar(ViewObjGVar);
  cfunc = ValAutoGVar(CustomViewGVar);

  /* if non-zero use this function, otherwise use `PrintObj'             */
  memcpy( readJmpError, TLS->readJmpError, sizeof(syJmp_buf) );
  if ( ! READ_ERROR() ) {
    if ( cfunc != 0 && TNUM_OBJ(cfunc) == T_FUNCTION ) {
      CALL_1ARGS(cfunc, obj);
    }
    else if ( func != 0 && TNUM_OBJ(func) == T_FUNCTION ) {
      ViewObj(obj);
    }
    else {
      PrintObj( obj );
    }
    Pr( "\n", 0L, 0L );
    memcpy( TLS->readJmpError, readJmpError, sizeof(syJmp_buf) );
  }
  else {
    memcpy( TLS->readJmpError, readJmpError, sizeof(syJmp_buf) );
  }
}


/****************************************************************************
**
*F  main( <argc>, <argv> )  . . . . . . .  main program, read-eval-print loop
*/
Obj AtExitFunctions;

Obj AlternativeMainLoop;

UInt SaveOnExitFileGVar;

UInt QUITTINGGVar;

Obj OnGapPromptHook;

Obj ErrorHandler;               /* not yet settable from GAP level */


typedef struct {
    const Char *                name;
    Obj *                       address;
} StructImportedGVars;

#ifndef MAX_IMPORTED_GVARS
#define MAX_IMPORTED_GVARS      1024
#endif

static StructImportedGVars ImportedGVars[MAX_IMPORTED_GVARS];
static Int NrImportedGVars;

static StructImportedGVars ImportedFuncs[MAX_IMPORTED_GVARS];
static Int NrImportedFuncs;

/* int restart_argc; 
   char **restart_argv; */

char *original_argv0;
static char **sysargv;
static char **sysenviron;

/* 
syJmp_buf SyRestartBuf;
*/

/* TL: Obj ShellContext = 0; */
/* TL: Obj BaseShellContext = 0; */
/* TL: UInt ShellContextDepth; */


Obj Shell ( Obj context, 
            UInt canReturnVoid,
            UInt canReturnObj,
            UInt lastDepth,
            UInt setTime,
            Char *prompt,
            Obj preCommandHook,
            UInt catchQUIT,
            Char *inFile,
            Char *outFile)
{
  UInt time = 0;
  UInt status;
  UInt oldindent;
  UInt oldPrintDepth;
  Obj res;
  Obj oldShellContext;
  Obj oldBaseShellContext;
  oldShellContext = TLS->ShellContext;
  TLS->ShellContext = context;
  oldBaseShellContext = TLS->BaseShellContext;
  TLS->BaseShellContext = context;
  TLS->ShellContextDepth = 0;
  
  /* read-eval-print loop                                                */
  if (!OpenOutput(outFile))
    ErrorMayQuit("SHELL: can't open outfile %s",(Int)outFile,0);

  if(!OpenInput(inFile))
    {
      CloseOutput();
      ErrorMayQuit("SHELL: can't open infile %s",(Int)inFile,0);
    }
  
  oldPrintDepth = TLS->PrintObjDepth;
  TLS->PrintObjDepth = 0;
  oldindent = TLS->output->indent;
  TLS->output->indent = 0;

  while ( 1 ) {

    /* start the stopwatch                                             */
    if (setTime)
      time = SyTime();

    /* read and evaluate one command                                   */
    TLS->prompt = prompt;
    ClearError();
    TLS->PrintObjDepth = 0;
    TLS->output->indent = 0;
      
    /* here is a hook: */
    if (preCommandHook) {
      if (!IS_FUNC(preCommandHook))
        {
                  Pr("#E CommandHook was non-function, ignoring\n",0L,0L);
        }
      else
        {
          Call0ArgsInNewReader(preCommandHook);
          /* Recover from a potential break loop: */
          TLS->prompt = prompt;
          ClearError();
        }
    }

    /* now  read and evaluate and view one command  */
    status = ReadEvalCommand(TLS->ShellContext);
    if (TLS->UserHasQUIT)
      break;


    /* handle ordinary command                                         */
    if ( status == STATUS_END && TLS->readEvalResult != 0 ) {

      /* remember the value in 'last'    */
      if (lastDepth >= 3)
        AssGVar( Last3, ValGVar( Last2 ) );
      if (lastDepth >= 2)
        AssGVar( Last2, ValGVar( Last  ) );
      if (lastDepth >= 1)
        AssGVar( Last,  TLS->readEvalResult   );

      /* print the result                                            */
      if ( ! TLS->dualSemicolon ) {
        ViewObjHandler( TLS->readEvalResult );
      }
            
    }

    /* handle return-value or return-void command                      */
    else if (status & STATUS_RETURN_VAL) 
      if(canReturnObj)
        break;
      else
        Pr( "'return <object>' cannot be used in this read-eval-print loop\n",
            0L, 0L );

    else if (status & STATUS_RETURN_VOID) 
      if(canReturnVoid ) 
        break;
      else
        Pr( "'return' cannot be used in this read-eval-print loop\n",
            0L, 0L );
    
    /* handle quit command or <end-of-file>                            */
    else if ( status & (STATUS_EOF | STATUS_QUIT ) ) {
      TLS->recursionDepth = 0;
      TLS->UserHasQuit = 1;
      break;
    }
        
    /* handle QUIT */
    else if (status & (STATUS_QQUIT)) {
      TLS->UserHasQUIT = 1;
      break;
    }
        
    /* stop the stopwatch                                          */
    if (setTime)
      AssGVar( Time, INTOBJ_INT( SyTime() - time ) );

    if (TLS->UserHasQuit)
      {
        FlushRestOfInputLine();
        TLS->UserHasQuit = 0;        /* quit has done its job if we are here */
      }

  }
  
  TLS->PrintObjDepth = oldPrintDepth;
  TLS->output->indent = oldindent;
  CloseInput();
  CloseOutput();
  TLS->BaseShellContext = oldBaseShellContext;
  TLS->ShellContext = oldShellContext;
  if (TLS->UserHasQUIT)
    {
      if (catchQUIT)
        {
          TLS->UserHasQUIT = 0;
          MakeReadWriteGVar(QUITTINGGVar);
          AssGVar(QUITTINGGVar, True);
          MakeReadOnlyGVar(QUITTINGGVar);
          return Fail;
        }
      else
        ReadEvalError();
    }

  if (status & (STATUS_EOF | STATUS_QUIT | STATUS_QQUIT))
    {
      return Fail;
    }
  if (status & STATUS_RETURN_VOID)
    {
      res = NEW_PLIST(T_PLIST_EMPTY,0);
      SET_LEN_PLIST(res,0);
      return res;
    }
  if (status & STATUS_RETURN_VAL)
    {
      res = NEW_PLIST(T_PLIST_HOM,1);
      SET_LEN_PLIST(res,1);
      SET_ELM_PLIST(res,1,TLS->readEvalResult);
      return res;
    }
  assert(0); 
  return (Obj) 0;
}



Obj FuncSHELL (Obj self, Obj args)
{
  Obj context = 0;
  UInt canReturnVoid = 0;
  UInt canReturnObj = 0;
  Int lastDepth = 0;
  UInt setTime = 0;
  Obj prompt = 0;
  Obj preCommandHook = 0;
  Obj infile;
  Obj outfile;
  Obj res;
  Char promptBuffer[81];
  UInt catchQUIT = 0;
  
  if (!IS_PLIST(args) || LEN_PLIST(args) != 10)
    ErrorMayQuit("SHELL takes 10 arguments",0,0);
  
  context = ELM_PLIST(args,1);
  if (TNUM_OBJ(context) != T_LVARS && TNUM_OBJ(context) != T_HVARS)
    ErrorMayQuit("SHELL: 1st argument should be a local variables bag",0,0);
  
  if (ELM_PLIST(args,2) == True)
    canReturnVoid = 1;
  else if (ELM_PLIST(args,2) == False)
    canReturnVoid = 0;
  else
    ErrorMayQuit("SHELL: 2nd argument (can return void) should be true or false",0,0);

  if (ELM_PLIST(args,3) == True)
    canReturnObj = 1;
  else if (ELM_PLIST(args,3) == False)
    canReturnObj = 0;
  else
    ErrorMayQuit("SHELL: 3rd argument (can return object) should be true or false",0,0);
  
  if (!IS_INTOBJ(ELM_PLIST(args,4)))
    ErrorMayQuit("SHELL: 4th argument (last depth) should be a small integer",0,0);
  lastDepth = INT_INTOBJ(ELM_PLIST(args,4));
  if (lastDepth < 0 )
    {
      Pr("#W SHELL: negative last depth treated as zero",0,0);
      lastDepth = 0;
    }
  else if (lastDepth > 3 )
    {
      Pr("#W SHELL: last depth greater than 3 treated as 3",0,0);
      lastDepth = 3;
    }

  if (ELM_PLIST(args,5) == True)
    setTime = 1;
  else if (ELM_PLIST(args,5) == False)
    setTime = 0;
  else
    ErrorMayQuit("SHELL: 5th argument (set time) should be true or false",0,0);
  
  prompt = ELM_PLIST(args,6);
  if (!IsStringConv(prompt) || GET_LEN_STRING(prompt) > 80)
    ErrorMayQuit("SHELL: 6th argument (prompt) must be a string of length at most 80 characters",0,0);
  promptBuffer[0] = '\0';
  SyStrncat(promptBuffer, CSTR_STRING(prompt), 80);

  preCommandHook = ELM_PLIST(args,7);
 
  if (preCommandHook == False)
    preCommandHook = 0;
  else if (!IS_FUNC(preCommandHook))
    ErrorMayQuit("SHELL: 7th argument (preCommandHook) must be function or false",0,0);

  
  infile = ELM_PLIST(args,8);
  if (!IsStringConv(infile))
    ErrorMayQuit("SHELL: 8th argument (infile) must be a string",0,0);

  outfile = ELM_PLIST(args,9);
  if (!IsStringConv(infile))
    ErrorMayQuit("SHELL: 9th argument (outfile) must be a string",0,0);

  if (ELM_PLIST(args,10) == True)
    catchQUIT = 1;
  else if (ELM_PLIST(args,10) == False)
    catchQUIT = 0;
  else
    ErrorMayQuit("SHELL: 10th argument (catch QUIT) should be true or false",0,0);
  res =  Shell(context, canReturnVoid, canReturnObj, lastDepth, setTime, promptBuffer, preCommandHook, catchQUIT,
               CSTR_STRING(infile), CSTR_STRING(outfile));

  TLS->UserHasQuit = 0;
  return res;
}



static void StrAppend(char **st, const char *st2)
{
    Int len,len2;
    if (*st == NULL)
        len = 0;
    else
        len = SyStrlen(*st);
    len2 = SyStrlen(st2);
    *st = realloc(*st,len+len2+1);
    if (*st == NULL) {
        printf("Extremely unexpected out of memory error. Giving up.\n");
        exit(1);
    }
    /* If *st was initially NULL, we must zero-terminate the
       newly allocated string. */
    if (len == 0) **st = 0;
    SyStrncat(*st,st2,len2);
}

#ifdef HAVE_REALPATH
static void DoFindMyself(char *myself, char **mypath, char **gappath)
{
    char *tmppath;
    char *p;

    /* First we find our own position in the filesystem: */
    *mypath = realpath(myself,NULL);
    if (*mypath == NULL) {
        printf("Could not determine my own path, giving up.\n");
        exit(-1);
    }
    tmppath = NULL;
    StrAppend(&tmppath,*mypath);
    p = tmppath+SyStrlen(tmppath);
    while (*p != '/') p--;
    *p = 0;
    StrAppend(&tmppath,"/../..");
    *gappath = realpath(tmppath,NULL);
    if (*gappath == NULL) {
        printf("Could not determine GAP path, giving up.\n");
        exit(-2);
    }
    free(tmppath);
}


int DoCreateStartupScript(int argc, char *argv[], int withws)
{
    /* This is used to create a startup shell script, possibly using
     * a saved workspace in a standard location. */
    /* We can use malloc/realloc here arbitrarily since this GAP
     * process will never start its memory manager before terminating! */
    char *mypath;
    char *gappath;
    char *tmppath;
    char *p;
    FILE *f;
    int i;

    DoFindMyself(argv[0],&mypath,&gappath);

    /* Now write out the startup script: */
    f = fopen(argv[2],"w");
    if (f == NULL) {
        printf("Could not write startup script to\n  %s\ngiving up.\n",argv[2]);
        return -3;
    }
    fprintf(f,"#!/bin/sh\n");
    fprintf(f,"# Created by %s\n",mypath);
    fprintf(f,"GAP_DIR=\"%s\"\n",gappath);
    fprintf(f,"GAP_PRG=\"%s\"\n",mypath);
    fprintf(f,"GAP_ARCH=\"%s\"\n",SYS_ARCH);
    tmppath = NULL;
    StrAppend(&tmppath,SYS_ARCH);
    p = tmppath;
    while (*p != 0 && *p != '/') p++;
    *p++ = 0;
    fprintf(f,"GAP_ARCH_SYS=\"%s\"\n",tmppath);
    fprintf(f,"GAP_ARCH_ABI=\"%s\"\n",p);	// FIXME: WRONG
    fprintf(f,"exec %s -l %s",mypath,gappath);
    if (withws) {
        tmppath[0] = 0;
        StrAppend(&tmppath,mypath);
        p = tmppath+SyStrlen(tmppath);
        while (*p != '/') p--;
        p[1] = 0;
        StrAppend(&tmppath,"workspace.gap");
        fprintf(f," -L %s",tmppath);
    }
    for (i = 3;i < argc;i++) fprintf(f," %s",argv[i]);
    fprintf(f," \"$@\"\n");
    fclose(f);
#ifdef HAVE_CHMOD
    chmod(argv[2],S_IRUSR | S_IWUSR | S_IXUSR |
                  S_IRGRP | S_IWGRP | S_IXGRP |
                  S_IROTH | S_IXOTH);
#else
    printf("Warning: Do not have chmod to make script executable!\n");
#endif
    free(tmppath);
    free(mypath);
    free(gappath);
    return 0;
}

int DoCreateWorkspace(char *myself)
{
    /* This is used to create an architecture-dependent saved
     * workspace in a standard location. */
    char *mypath;
    char *gappath;
    char *command;
    char *tmppath;
    char *p;
    FILE *f;

    DoFindMyself(myself,&mypath,&gappath);

    /* Now we create a saved workspace: */
    printf("Creating workspace...\n");
    command = NULL;
    StrAppend(&command,mypath);
    StrAppend(&command," -N -r");
    StrAppend(&command," -l ");
    StrAppend(&command,gappath);

    tmppath = NULL;
    StrAppend(&tmppath,mypath);
    p = tmppath+SyStrlen(tmppath);
    while (*p != '/') p--;
    p[1] = 0;
    StrAppend(&tmppath,"workspace.gap");

    /* Now to the action: */
    f = popen(command,"w");
    if (f == NULL) {
        printf("Could not start myself to save workspace, giving up.\n");
        return -6;
    }
    fprintf(f,"??blabla\n");
    fprintf(f,"SaveWorkspace(\"%s\");\n",tmppath);
    fprintf(f,"quit;\n");
    fflush(f);
    pclose(f);
    printf("\nDone creating workspace in\n  %s\n",tmppath);

    free(tmppath);
    free(command);
    free(gappath);
    free(mypath);

    return 0;
}

int DoFixGac(char *myself)
{
    char *mypath;
    char *gappath;
    FILE *f;
    char *gacpath;
    char *gapbin;
    char *newpath;
    char *p,*q,*r;
    char *buf,*buf2;
    size_t len,written;

    DoFindMyself(myself,&mypath,&gappath);
    gacpath = NULL;
    StrAppend(&gacpath,mypath);
    p = gacpath + SyStrlen(gacpath);
    while (*p != '/') p--;
    *p = 0;
    gapbin = NULL;
    StrAppend(&gapbin,gacpath);
    StrAppend(&gacpath,"/gac");
    newpath = NULL;
    StrAppend(&newpath,gacpath);
    StrAppend(&newpath,".new");
    f = fopen(gacpath,"r");
    if (f == NULL) {
        printf("Could not open gac. Giving up.\n");
        return -7;
    }
    buf = malloc(65536);
    buf2 = malloc(65536+SyStrlen(gapbin)+10);
    if (buf == NULL || buf2 == NULL) {
        printf("Could not allocate 128kB of memory. Giving up.\n");
        return -8;
    }
    len = fread(buf,1,65534,f);
    fclose(f);

    /* Now manipulate it: */
    p = buf;
    p[len] = 0;
    p[len+1] = 0;
    q = buf2;
    while (*p) {
        if (!SyStrncmp(p,"gap_bin=",8)) {
            while (*p != '\n' && *p != 0) p++;
            *q++ = 'g'; *q++ = 'a'; *q++ = 'p'; *q++ = '_';
            *q++ = 'b'; *q++ = 'i'; *q++ = 'n'; *q++ = '=';
            r = gapbin;
            while (*r) *q++ = *r++;
            *q++ = '\n';
        } else {
            while (*p != '\n' && *p != 0) *q++ = *p++;
            *q++ = *p++;
        }
    }
    len = q - buf2;

    f = fopen(newpath,"w");
    if (f == NULL) {
        printf("Could not open gac.new. Giving up.\n");
        return -9;
    }
    written = fwrite(buf2,1,len,f);
    if (written < len) {
        printf("Could not write gac.new. Giving up.\n");
        fclose(f);
        return -10;
    }
    if (fclose(f) < 0) {
        printf("Could not close gac.new. Giving up.\n");
        fclose(f);
        return -11;
    }
    if (rename(newpath,gacpath) < 0) {
        printf("Could not replace gac with new version. Giving up.\n");
        return -12;
    }
    return 0;
}
#endif

/****************************************************************************
**
*V  SystemErrorCode . . . . . . . . . integer error code with which GAP exits
**
*/

static int SystemErrorCode = 0;

int realmain (
	  int                 argc,
	  char *              argv [],
          char *              environ [] );

int main (
	  int                 argc,
	  char *              argv [],
          char *              environ [] )
{
#ifdef PRINT_BACKTRACE
  void InstallBacktraceHandlers();
  InstallBacktraceHandlers();
#endif
  RunThreadedMain(realmain, argc, argv, environ);
  return 0;
}

#define main realmain

int main (
          int                 argc,
          char *              argv [],
          char *              environ [] )
{
  UInt                type;                   /* result of compile       */
  Obj                 func;                   /* function (compiler)     */
  Int4                crc;                    /* crc of file to compile  */

#ifdef HAVE_REALPATH
  if (argc >= 3 && !SyStrcmp(argv[1],"--createstartupscript")) {
      return DoCreateStartupScript(argc,argv,0);
  }
  if (argc >= 3 && !SyStrcmp(argv[1],"--createstartupscriptwithws")) {
      return DoCreateStartupScript(argc,argv,1);
  }
  if (argc >= 2 && !SyStrcmp(argv[1],"--createworkspace")) {
      return DoCreateWorkspace(argv[0]);
  }
  if (argc >= 2 && !SyStrcmp(argv[1],"--fixgac")) {
      return DoFixGac(argv[0]);
  }
#endif
  
  original_argv0 = argv[0];
  sysargv = argv;
  sysenviron = environ;
  
  /* prepare for a possible restart 
  if (setjmp(SyRestartBuf))
    {
      argc = restart_argc;
      argv = restart_argv;
    }
    `*/

  /* Initialize assorted variables in this file */
  /*   BreakOnError = 1;
       ErrorCount = 0; */
  NrImportedGVars = 0;
  NrImportedFuncs = 0;
  ErrorHandler = (Obj) 0;
  TLS->UserHasQUIT = 0;
  TLS->UserHasQuit = 0;
    
  /* initialize everything and read init.g which runs the GAP session */
  InitializeGap( &argc, argv );
  if (!TLS->UserHasQUIT) {         /* maybe the user QUIT from the initial
                                   read of init.g  somehow*/
    /* maybe compile in which case init.g got skipped */
    if ( SyCompilePlease ) {
      if ( ! OpenInput(SyCompileInput) ) {
        SyExit(1);
      }
      func = READ_AS_FUNC();
      crc  = SyGAPCRC(SyCompileInput);
      if (SyStrlen(SyCompileOptions) != 0)
        SetCompileOpts(SyCompileOptions);
      type = CompileFunc(
                         SyCompileOutput,
                         func,
                         SyCompileName,
                         crc,
                         SyCompileMagic1 );
      if ( type == 0 )
        SyExit( 1 );
      SyExit( 0 );
    }
  }
  SyExit(SystemErrorCode);
  return 0;
}

/****************************************************************************
**
*F  FuncRESTART_GAP( <self>, <cmdline> ) . . . . . . . .  restart gap
**
*/

Char *restart_argv_buffer[1000];

#include <unistd.h> /* move this and wrap execvp later */

Obj FuncRESTART_GAP( Obj self, Obj cmdline )
{
  Char *s, *f,  **v;
  while (!IsStringConv(cmdline))
    {
      cmdline = ErrorReturnObj("RESTART_GAP: <cmdline> must be a string, not a %s",
                               (Int) TNAM_OBJ(cmdline), (Int) 0,
                               "You can resturn a string to continue");
    }
  s = CSTR_STRING(cmdline);
  /* Pr("%s\n",(Int)s, 0); */
  f = s;
  v = restart_argv_buffer;
  while (*s) {
    *v++ = s;
    while (*s && !IsSpace(*s))
      s++;
    while (IsSpace(*s))
      *s++ = '\0';
  }
  *v = (Char *)0;
  /*  restart_argc = ct;
      restart_argv = restart_argv_buffer; */
  /* FinishBags(); */
  execvp(f,restart_argv_buffer);
  /*  longjmp(SyRestartBuf,1); */
  return Fail; /* shouldn't normally get here */
}



/****************************************************************************
**
*F  FuncID_FUNC( <self>, <val1> ) . . . . . . . . . . . . . . . return <val1>
*/
Obj FuncID_FUNC (
                 Obj                 self,
                 Obj                 val1 )
{
  return val1;
}


/****************************************************************************
**
*F  FuncRuntime( <self> ) . . . . . . . . . . . . internal function 'Runtime'
**
**  'FuncRuntime' implements the internal function 'Runtime'.
**
**  'Runtime()'
**
**  'Runtime' returns the time spent since the start of GAP in  milliseconds.
**  How much time execution of statements take is of course system dependent.
**  The accuracy of this number is also system dependent.
*/
Obj FuncRuntime (
                 Obj                 self )
{
  return INTOBJ_INT( SyTime() );
}



Obj FuncRUNTIMES( Obj     self)
{
  Obj    res;
#if HAVE_GETRUSAGE
  res = NEW_PLIST(T_PLIST, 4);
  SET_LEN_PLIST(res, 4);
  SET_ELM_PLIST(res, 1, INTOBJ_INT( SyTime() ));
  SET_ELM_PLIST(res, 2, INTOBJ_INT( SyTimeSys() ));
  SET_ELM_PLIST(res, 3, INTOBJ_INT( SyTimeChildren() ));
  SET_ELM_PLIST(res, 4, INTOBJ_INT( SyTimeChildrenSys() ));
#else
  res = INTOBJ_INT( SyTime() );
#endif
  return res;
   
}



/****************************************************************************
**
*F  FuncSizeScreen( <self>, <args> )  . . . .  internal function 'SizeScreen'
**
**  'FuncSizeScreen'  implements  the  internal  function 'SizeScreen' to get
**  or set the actual screen size.
**
**  'SizeScreen()'
**
**  In this form 'SizeScreen'  returns the size of the  screen as a list with
**  two  entries.  The first is  the length of each line,  the  second is the
**  number of lines.
**
**  'SizeScreen( [ <x>, <y> ] )'
**
**  In this form 'SizeScreen' sets the size of the screen.  <x> is the length
**  of each line, <y> is the number of lines.  Either value may  be  missing,
**  to leave this value unaffected.  Note that those parameters can  also  be
**  set with the command line options '-x <x>' and '-y <y>'.
*/
Obj FuncSizeScreen (
                    Obj                 self,
                    Obj                 args )
{
  Obj                 size;           /* argument and result list        */
  Obj                 elm;            /* one entry from size             */
  UInt                len;            /* length of lines on the screen   */
  UInt                nr;             /* number of lines on the screen   */

  /* check the arguments                                                 */
  while ( ! IS_SMALL_LIST(args) || 1 < LEN_LIST(args) ) {
    args = ErrorReturnObj(
                          "Function: number of arguments must be 0 or 1 (not %d)",
                          LEN_LIST(args), 0L,
                          "you can replace the argument list <args> via 'return <args>;'" );
  }

  /* get the arguments                                                   */
  if ( LEN_LIST(args) == 0 ) {
    size = NEW_PLIST( T_PLIST, 0 );
    SET_LEN_PLIST( size, 0 );
  }

  /* otherwise check the argument                                        */
  else {
    size = ELM_LIST( args, 1 );
    while ( ! IS_SMALL_LIST(size) || 2 < LEN_LIST(size) ) {
      size = ErrorReturnObj(
                            "SizeScreen: <size> must be a list of length 2",
                            0L, 0L,
                            "you can replace <size> via 'return <size>;'" );
    }
  }

  /* extract the length                                                  */
  if ( LEN_LIST(size) < 1 || ELM0_LIST(size,1) == 0 ) {
    len = 0;
  }
  else {
    elm = ELMW_LIST(size,1);
    while ( TNUM_OBJ(elm) != T_INT ) {
      elm = ErrorReturnObj(
                           "SizeScreen: <x> must be an integer",
                           0L, 0L,
                           "you can replace <x> via 'return <x>;'" );
    }
    len = INT_INTOBJ( elm );
    if ( len < 20  )  len = 20;
    if ( MAXLENOUTPUTLINE < len )  len = MAXLENOUTPUTLINE;
  }

  /* extract the number                                                  */
  if ( LEN_LIST(size) < 2 || ELM0_LIST(size,2) == 0 ) {
    nr = 0;
  }
  else {
    elm = ELMW_LIST(size,2);
    while ( TNUM_OBJ(elm) != T_INT ) {
      elm = ErrorReturnObj(
                           "SizeScreen: <y> must be an integer",
                           0L, 0L,
                           "you can replace <y> via 'return <y>;'" );
    }
    nr = INT_INTOBJ( elm );
    if ( nr < 10 )  nr = 10;
  }

  /* set length and number                                               */
  if (len != 0)
    {
      SyNrCols = len;
      SyNrColsLocked = 1;
    }
  if (nr != 0)
    {
      SyNrRows = nr;
      SyNrRowsLocked = 1;
    }

  /* make and return the size of the screen                              */
  size = NEW_PLIST( T_PLIST, 2 );
  SET_LEN_PLIST( size, 2 );
  SET_ELM_PLIST( size, 1, INTOBJ_INT(SyNrCols) );
  SET_ELM_PLIST( size, 2, INTOBJ_INT(SyNrRows)  );
  return size;

}


/****************************************************************************
**
*F  FuncWindowCmd( <self>, <args> ) . . . . . . . .  execute a window command
*/
static Obj WindowCmdString;

Obj FuncWindowCmd (
                   Obj              self,
                   Obj             args )
{
  Obj             tmp;
  Obj               list;
  Int             len;
  Int             n,  m;
  Int             i;
  Char *          ptr;
  Char *          qtr;

  /* check arguments                                                     */
  while ( ! IS_SMALL_LIST(args) ) {
    args = ErrorReturnObj( "argument list must be a list (not a %s)",
                           (Int)TNAM_OBJ(args), 0L,
                           "you can replace the argument list <args> via 'return <args>;'" );

  }
  tmp = ELM_LIST(args,1);
  while ( ! IsStringConv(tmp) || 3 != LEN_LIST(tmp) ) {
    while ( ! IsStringConv(tmp) ) {
      tmp = ErrorReturnObj( "<cmd> must be a string (not a %s)",
                            (Int)TNAM_OBJ(tmp), 0L,
                            "you can replace <cmd> via 'return <cmd>;'" );
    }
    if ( 3 != LEN_LIST(tmp) ) {
      tmp = ErrorReturnObj( "<cmd> must be a string of length 3",
                            0L, 0L,
                            "you can replace <cmd> via 'return <cmd>;'" );
    }
  }

  /* compute size needed to store argument string                        */
  len = 13;
  for ( i = 2;  i <= LEN_LIST(args);  i++ )
    {
      tmp = ELM_LIST( args, i );
      while ( TNUM_OBJ(tmp) != T_INT && ! IsStringConv(tmp) ) {
        tmp = ErrorReturnObj(
                             "%d. argument must be a string or integer (not a %s)",
                             i, (Int)TNAM_OBJ(tmp),
                             "you can replace the argument <arg> via 'return <arg>;'" );
        SET_ELM_PLIST( args, i, tmp );
      }
      if ( TNUM_OBJ(tmp) == T_INT )
        len += 12;
      else
        len += 12 + LEN_LIST(tmp);
    }
  if ( SIZE_OBJ(WindowCmdString) <= len ) {
    ResizeBag( WindowCmdString, 2*len+1 );
  }

  /* convert <args> into an argument string                              */
  ptr  = (Char*) CSTR_STRING(WindowCmdString);
  *ptr = '\0';

  /* first the command name                                              */
  SyStrncat( ptr, CSTR_STRING( ELM_LIST(args,1) ), 3 );
  ptr += 3;

  /* and now the arguments                                               */
  for ( i = 2;  i <= LEN_LIST(args);  i++ )
    {
      tmp = ELM_LIST(args,i);

      if ( TNUM_OBJ(tmp) == T_INT ) {
        *ptr++ = 'I';
        m = INT_INTOBJ(tmp);
        for ( m = (m<0)?-m:m;  0 < m;  m /= 10 )
          *ptr++ = (m%10) + '0';
        if ( INT_INTOBJ(tmp) < 0 )
          *ptr++ = '-';
        else
          *ptr++ = '+';
      }
      else {
        *ptr++ = 'S';
        m = LEN_LIST(tmp);
        for ( ; 0 < m;  m/= 10 )
          *ptr++ = (m%10) + '0';
        *ptr++ = '+';
        qtr = CSTR_STRING(tmp);
        for ( m = LEN_LIST(tmp);  0 < m;  m-- )
          *ptr++ = *qtr++;
      }
    }
  *ptr = 0;

  /* now call the window front end with the argument string              */
  qtr = CSTR_STRING(WindowCmdString);
  ptr = SyWinCmd( qtr, SyStrlen(qtr) );
  len = SyStrlen(ptr);

  /* now convert result back into a list                                 */
  list = NEW_PLIST( T_PLIST, 11 );
  SET_LEN_PLIST( list, 0 );
  i = 1;
  while ( 0 < len ) {
    if ( *ptr == 'I' ) {
      ptr++;
      for ( n=0,m=1; '0' <= *ptr && *ptr <= '9'; ptr++,m *= 10,len-- )
        n += (*ptr-'0') * m;
      if ( *ptr++ == '-' )
        n *= -1;
      len -= 2;
      AssPlist( list, i, INTOBJ_INT(n) );
    }
    else if ( *ptr == 'S' ) {
      ptr++;
      for ( n=0,m=1;  '0' <= *ptr && *ptr <= '9';  ptr++,m *= 10,len-- )
        n += (*ptr-'0') * m;
      ptr++; /* ignore the '+' */
      /*CCC tmp = NEW_STRING(n);
      *CSTR_STRING(tmp) = '\0';
      SyStrncat( CSTR_STRING(tmp), ptr, n ); CCC*/
      C_NEW_STRING(tmp, n, ptr);
      ptr += n;
      len -= n+2;
      AssPlist( list, i, tmp );
    }
    else {
      ErrorQuit( "unknown return value '%s'", (Int)ptr, 0 );
      return 0;
    }
    i++;
  }

  /* if the first entry is one signal an error */
  if ( ELM_LIST(list,1) == INTOBJ_INT(1) ) {
    /*CCCtmp = NEW_STRING(15);
      SyStrncat( CSTR_STRING(tmp), "window system: ", 15 );CCC*/
    C_NEW_STRING(tmp, 15, "window system: ");  
    SET_ELM_PLIST( list, 1, tmp );
    SET_LEN_PLIST( list, i-1 );
    return CALL_XARGS(Error,list);
    /*     return FuncError( 0, list );*/
  }
  else {
    for ( m = 1;  m <= i-2;  m++ )
      SET_ELM_PLIST( list, m, ELM_PLIST(list,m+1) );
    SET_LEN_PLIST( list, i-2 );
    return list;
  }
}


/****************************************************************************
**

*F * * * * * * * * * * * * * * error functions * * * * * * * * * * * * * * *
*/



/****************************************************************************
**

*F  FuncDownEnv( <self>, <level> )  . . . . . . . . .  change the environment
*/

/* Obj  ErrorLVars0;    */
/* TL: Obj  ErrorLVars; */
/* TL: Int  ErrorLLevel; */

/* TL: extern Obj BottomLVars; */


void DownEnvInner( Int depth )
{
  /* if we really want to go up                                          */
  if ( depth < 0 && -TLS->ErrorLLevel <= -depth ) {
    depth = 0;
    TLS->errorLVars = TLS->ErrorLVars0;
    TLS->ErrorLLevel = 0;
    TLS->ShellContextDepth = 0;
    TLS->ShellContext = TLS->BaseShellContext;
  }
  else if ( depth < 0 ) {
    depth = -TLS->ErrorLLevel + depth;
    TLS->errorLVars = TLS->ErrorLVars0;
    TLS->ErrorLLevel = 0;
    TLS->ShellContextDepth = 0;
    TLS->ShellContext = TLS->BaseShellContext;
  }
  
  /* now go down                                                         */
  while ( 0 < depth
          && TLS->errorLVars != TLS->bottomLVars
          && PTR_BAG(TLS->errorLVars)[2] != TLS->bottomLVars ) {
    TLS->errorLVars = PTR_BAG(TLS->errorLVars)[2];
    TLS->ErrorLLevel--;
    TLS->ShellContext = PTR_BAG(TLS->ShellContext)[2];
    TLS->ShellContextDepth--;
    depth--;
  }
}
  
Obj FuncDownEnv (
                 Obj                 self,
                 Obj                 args )
{
  Int                 depth;

  if ( LEN_LIST(args) == 0 ) {
    depth = 1;
  }
  else if ( LEN_LIST(args) == 1 && IS_INTOBJ( ELM_PLIST(args,1) ) ) {
    depth = INT_INTOBJ( ELM_PLIST( args, 1 ) );
  }
  else {
    ErrorQuit( "usage: DownEnv( [ <depth> ] )", 0L, 0L );
    return 0;
  }
  if ( TLS->errorLVars == 0 ) {
    Pr( "not in any function\n", 0L, 0L );
    return 0;
  }

  DownEnvInner( depth);

  /* return nothing                                                      */
  return 0;
}

Obj FuncUpEnv (
               Obj                 self,
               Obj                 args )
{
  Int                 depth;
  if ( LEN_LIST(args) == 0 ) {
    depth = 1;
  }
  else if ( LEN_LIST(args) == 1 && IS_INTOBJ( ELM_PLIST(args,1) ) ) {
    depth = INT_INTOBJ( ELM_PLIST( args, 1 ) );
  }
  else {
    ErrorQuit( "usage: UpEnv( [ <depth> ] )", 0L, 0L );
    return 0;
  }
  if ( TLS->errorLVars == 0 ) {
    Pr( "not in any function\n", 0L, 0L );
    return 0;
  }

  DownEnvInner(-depth);
  return 0;
}


Obj FuncPrintExecutingStatement(Obj self, Obj context)
{
  Obj currLVars = TLS->currLVars;
  Expr call;
  if (context == TLS->bottomLVars)
    return (Obj) 0;
  SWITCH_TO_OLD_LVARS(context);
  call = BRK_CALL_TO();
  if ( call == 0 ) {
    Pr( "<compiled or corrupted statement> ", 0L, 0L );
  }
#if T_PROCCALL_0ARGS
    else if ( FIRST_STAT_TNUM <= TNUM_STAT(call)
              && TNUM_STAT(call)  <= LAST_STAT_TNUM ) {
#else
     else if ( TNUM_STAT(call)  <= LAST_STAT_TNUM ) {
#endif
      PrintStat( call );
    }
    else if ( FIRST_EXPR_TNUM <= TNUM_EXPR(call)
              && TNUM_EXPR(call)  <= LAST_EXPR_TNUM ) {
      PrintExpr( call );
    }
    SWITCH_TO_OLD_LVARS( currLVars );
    return (Obj) 0;
}    

/****************************************************************************
**
*F  FuncCallFuncTrapError( <self>, <func> )
**
*/
  
/* syJmp_buf CatchBuffer; */
/* TL: Obj ThrownObject = 0; */

Obj FuncCALL_WITH_CATCH( Obj self, Obj func, Obj args )
{
    syJmp_buf readJmpError;
    Obj plain_args;
    Obj res;
    Obj currLVars;
    Obj result;
    Stat currStat;
    int lockSP;
    if (!IS_FUNC(func))
      ErrorMayQuit("CALL_WITH_CATCH(<func>,<args>): <func> must be a function",0,0);
    if (!IS_LIST(args))
      ErrorMayQuit("CALL_WITH_CATCH(<func>,<args>): <args> must be a list",0,0);
    if (!IS_PLIST(args))
      {
        plain_args = SHALLOW_COPY_OBJ(args);
        PLAIN_LIST(plain_args);
      }
    else 
      plain_args = args;
    memcpy((void *)&readJmpError, (void *)&TLS->readJmpError, sizeof(syJmp_buf));
    currLVars = TLS->currLVars;
    currStat = TLS->currStat;
    res = NEW_PLIST(T_PLIST_DENSE+IMMUTABLE,2);
    lockSP = RegionLockSP();
    if (sySetjmp(TLS->readJmpError)) {
      SET_LEN_PLIST(res,2);
      SET_ELM_PLIST(res,1,False);
      SET_ELM_PLIST(res,2,TLS->thrownObject);
      CHANGED_BAG(res);
      TLS->thrownObject = 0;
      TLS->currLVars = currLVars;
      TLS->ptrLVars = PTR_BAG(TLS->currLVars);
      TLS->ptrBody = (Stat*)PTR_BAG(BODY_FUNC(CURR_FUNC));
      TLS->currStat = currStat;
      PopRegionLocks(lockSP);
      if (TLS->CurrentHashLock)
        HashUnlock(TLS->CurrentHashLock);
    } else {
      switch (LEN_PLIST(plain_args)) {
      case 0: result = CALL_0ARGS(func);
        break;
      case 1: result = CALL_1ARGS(func, ELM_PLIST(plain_args,1));
        break;
      case 2: result = CALL_2ARGS(func, ELM_PLIST(plain_args,1),
                                  ELM_PLIST(plain_args,2));
        break;
      case 3: result = CALL_3ARGS(func, ELM_PLIST(plain_args,1),
                                  ELM_PLIST(plain_args,2), ELM_PLIST(plain_args,3));
        break;
      case 4: result = CALL_4ARGS(func, ELM_PLIST(plain_args,1),
                                  ELM_PLIST(plain_args,2), ELM_PLIST(plain_args,3),
                                  ELM_PLIST(plain_args,4));
        break;
      case 5: result = CALL_5ARGS(func, ELM_PLIST(plain_args,1),
                                  ELM_PLIST(plain_args,2), ELM_PLIST(plain_args,3),
                                  ELM_PLIST(plain_args,4), ELM_PLIST(plain_args,5));
        break;
      case 6: result = CALL_6ARGS(func, ELM_PLIST(plain_args,1),
                                  ELM_PLIST(plain_args,2), ELM_PLIST(plain_args,3),
                                  ELM_PLIST(plain_args,4), ELM_PLIST(plain_args,5),
                                  ELM_PLIST(plain_args,6));
        break;
      default: result = CALL_XARGS(func, plain_args);
      }
      /* There should be no locks to pop off the stack, but better safe than sorry. */
      PopRegionLocks(lockSP);
      SET_ELM_PLIST(res,1,True);
      if (result)
        {
          SET_LEN_PLIST(res,2);
          SET_ELM_PLIST(res,2,result);
          CHANGED_BAG(res);
        }
      else
        SET_LEN_PLIST(res,1);
    }
    memcpy((void *)&TLS->readJmpError, (void *)&readJmpError, sizeof(syJmp_buf));
    return res;
}

Obj FuncJUMP_TO_CATCH( Obj self, Obj payload)
{
  TLS->thrownObject = payload;
  syLongjmp(TLS->readJmpError, 1);
  return 0;
}
  

#if 0
TL: UInt UserHasQuit;
TL: UInt UserHasQUIT; 
#endif

Obj FuncSetUserHasQuit( Obj Self, Obj value)
{
  TLS->UserHasQuit = INT_INTOBJ(value);
  if (TLS->UserHasQuit)
    TLS->recursionDepth = 0;
  return 0;
}

/****************************************************************************
**
*F  ErrorQuit( <msg>, <arg1>, <arg2> )  . . . . . . . . . . .  print and quit
*/

static Obj ErrorMessageToGAPString( 
    const Char *        msg,
    Int                 arg1,
    Int                 arg2 )
{
  Char message[500];
  Obj Message;
  SPrTo(message, sizeof(message), msg, arg1, arg2);
  message[sizeof(message)-1] = '\0';
  C_NEW_STRING(Message, SyStrlen(message), message); 
  return Message;
}

Obj CallErrorInner (
    const Char *        msg,
    Int                 arg1,
    Int                 arg2,
    UInt                justQuit,
    UInt                mayReturnVoid,
    UInt                mayReturnObj,
    Obj                 lateMessage,
    UInt                printThisStatement)
{
  Obj EarlyMsg;
  Obj r = NEW_PREC(0);
  Obj l;
  Obj result;
  EarlyMsg = ErrorMessageToGAPString(msg, arg1, arg2);
  AssPRec(r, RNamName("context"), TLS->currLVars);
  AssPRec(r, RNamName("justQuit"), justQuit? True : False);
  AssPRec(r, RNamName("mayReturnObj"), mayReturnObj? True : False);
  AssPRec(r, RNamName("mayReturnVoid"), mayReturnVoid? True : False);
  AssPRec(r, RNamName("printThisStatement"), printThisStatement? True : False);
  AssPRec(r, RNamName("lateMessage"), lateMessage);
  l = NEW_PLIST(T_PLIST_HOM+IMMUTABLE, 1);
  SET_ELM_PLIST(l,1,EarlyMsg);
  SET_LEN_PLIST(l,1);
  SET_BRK_CALL_TO(TLS->currStat);
  result = CALL_2ARGS(ErrorInner,r,l);  
  return result;
}

void ErrorQuit (
    const Char *        msg,
    Int                 arg1,
    Int                 arg2 )
{
  CallErrorInner(msg, arg1, arg2, 1, 0, 0, False, 1);
}


/****************************************************************************
**
*F  ErrorQuitBound( <name> )  . . . . . . . . . . . . . . .  unbound variable
*/
void ErrorQuitBound (
    const Char *        name )
{
    ErrorQuit(
        "variable '%s' must have an assigned value",
        (Int)name, 0L );
}


/****************************************************************************
**
*F  ErrorQuitFuncResult() . . . . . . . . . . . . . . . . must return a value
*/
void ErrorQuitFuncResult ( void )
{
    ErrorQuit(
        "function must return a value",
        0L, 0L );
}


/****************************************************************************
**
*F  ErrorQuitIntSmall( <obj> )  . . . . . . . . . . . . . not a small integer
*/
void ErrorQuitIntSmall (
    Obj                 obj )
{
    ErrorQuit(
        "<obj> must be a small integer (not a %s)",
        (Int)TNAM_OBJ(obj), 0L );
}


/****************************************************************************
**
*F  ErrorQuitIntSmallPos( <obj> ) . . . . . . .  not a positive small integer
*/
void ErrorQuitIntSmallPos (
    Obj                 obj )
{
    ErrorQuit(
        "<obj> must be a positive small integer (not a %s)",
        (Int)TNAM_OBJ(obj), 0L );
}

/****************************************************************************
**
*F  ErrorQuitIntPos( <obj> ) . . . . . . .  not a positive small integer
*/
void ErrorQuitIntPos (
    Obj                 obj )
{
    ErrorQuit(
        "<obj> must be a positive integer (not a %s)",
        (Int)TNAM_OBJ(obj), 0L );
}


/****************************************************************************
**
*F  ErrorQuitBool( <obj> )  . . . . . . . . . . . . . . . . . . not a boolean
*/
void ErrorQuitBool (
    Obj                 obj )
{
    ErrorQuit(
        "<obj> must be 'true' or 'false' (not a %s)",
        (Int)TNAM_OBJ(obj), 0L );
}


/****************************************************************************
**
*F  ErrorQuitFunc( <obj> )  . . . . . . . . . . . . . . . . .  not a function
*/
void ErrorQuitFunc (
    Obj                 obj )
{
    ErrorQuit(
        "<obj> must be a function (not a %s)",
        (Int)TNAM_OBJ(obj), 0L );
}


/****************************************************************************
**
*F  ErrorQuitNrArgs( <narg>, <args> ) . . . . . . . wrong number of arguments
*/
void ErrorQuitNrArgs (
    Int                 narg,
    Obj                 args )
{
    ErrorQuit(
        "Function Calls: number of arguments must be %d (not %d)",
        narg, LEN_PLIST( args ) );
}

/****************************************************************************
**
*F  ErrorQuitRange3( <first>, <second>, <last> ) . . divisibility
*/
void ErrorQuitRange3 (
                      Obj                 first,
                      Obj                 second,
                      Obj                 last)
{
    ErrorQuit(
        "Range expression <last>-<first> must be divisible by <second>-<first>, not %d %d",
        INT_INTOBJ(last)-INT_INTOBJ(first), INT_INTOBJ(second)-INT_INTOBJ(first) );
}


/****************************************************************************
**
*F  ErrorReturnObj( <msg>, <arg1>, <arg2>, <msg2> ) . .  print and return obj
*/
Obj ErrorReturnObj (
    const Char *        msg,
    Int                 arg1,
    Int                 arg2,
    const Char *        msg2 )
{
  Obj LateMsg;
  C_NEW_STRING(LateMsg, SyStrlen(msg2), msg2);
  return CallErrorInner(msg, arg1, arg2, 0, 0, 1, LateMsg, 1);
}


/****************************************************************************
**
*F  ErrorReturnVoid( <msg>, <arg1>, <arg2>, <msg2> )  . . .  print and return
*/
void ErrorReturnVoid (
    const Char *        msg,
    Int                 arg1,
    Int                 arg2,
    const Char *        msg2 )
{
  Obj LateMsg;
  C_NEW_STRING(LateMsg, SyStrlen(msg2), msg2);
  CallErrorInner( msg, arg1, arg2, 0,1,0,LateMsg, 1);
  /*    ErrorMode( msg, arg1, arg2, (Obj)0, msg2, 'x' ); */
}

/****************************************************************************
**
*F  ErrorMayQuit( <msg>, <arg1>, <arg2> )  . . .  print and return
*/
void ErrorMayQuit (
    const Char *        msg,
    Int                 arg1,
    Int                 arg2)
{
  CallErrorInner(msg, arg1, arg2, 0, 0,0, False, 1);
 
}

Obj Error;
Obj ErrorInner;


/****************************************************************************
**

*F * * * * * * * * * functions for creating the init file * * * * * * * * * *
*/

/* deleted 9/5/11 */

/*************************************************************************
**

*F * * * * * * * * * functions for dynamical/static modules * * * * * * * * *
*/



/****************************************************************************
**

*F  FuncGAP_CRC( <self>, <name> ) . . . . . . . create a crc value for a file
*/
Obj FuncGAP_CRC (
    Obj                 self,
    Obj                 filename )
{
    /* check the argument                                                  */
    while ( ! IsStringConv( filename ) ) {
        filename = ErrorReturnObj(
            "<filename> must be a string (not a %s)",
            (Int)TNAM_OBJ(filename), 0L,
            "you can replace <filename> via 'return <filename>;'" );
    }

    /* compute the crc value                                               */
    return INTOBJ_INT( SyGAPCRC( CSTR_STRING(filename) ) );
}


/****************************************************************************
**
*F  FuncLOAD_DYN( <self>, <name>, <crc> ) . . .  try to load a dynamic module
*/
Obj FuncLOAD_DYN (
    Obj                 self,
    Obj                 filename,
    Obj                 crc )
{
    InitInfoFunc        init;
    StructInitInfo *    info;
    Obj                 crc1;
    Int                 res;

    /* check the argument                                                  */
    while ( ! IsStringConv( filename ) ) {
        filename = ErrorReturnObj(
            "<filename> must be a string (not a %s)",
            (Int)TNAM_OBJ(filename), 0L,
            "you can replace <filename> via 'return <filename>;'" );
    }
    while ( ! IS_INTOBJ(crc) && crc!=False ) {
        crc = ErrorReturnObj(
            "<crc> must be a small integer or 'false' (not a %s)",
            (Int)TNAM_OBJ(crc), 0L,
            "you can replace <crc> via 'return <crc>;'" );
    }

    /* try to read the module                                              */
    init = SyLoadModule( CSTR_STRING(filename) );
    if ( (Int)init == 1 )
        ErrorQuit( "module '%s' not found", (Int)CSTR_STRING(filename), 0L );
    else if ( (Int) init == 3 )
        ErrorQuit( "symbol 'Init_Dynamic' not found", 0L, 0L );
    else if ( (Int) init == 5 )
        ErrorQuit( "forget symbol failed", 0L, 0L );

    /* no dynamic library support                                          */
    else if ( (Int) init == 7 ) {
        if ( SyDebugLoading ) {
            Pr( "#I  LOAD_DYN: no support for dynamical loading\n", 0L, 0L );
        }
        return False; 
    }

    /* get the description structure                                       */
    info = (*init)();
    if ( info == 0 )
        ErrorQuit( "call to init function failed", 0L, 0L );

    /* check the crc value                                                 */
    if ( crc != False ) {
        crc1 = INTOBJ_INT( info->crc );
        if ( ! EQ( crc, crc1 ) ) {
            if ( SyDebugLoading ) {
                Pr( "#I  LOAD_DYN: crc values do not match, gap ", 0L, 0L );
                PrintInt( crc );
                Pr( ", dyn ", 0L, 0L );
                PrintInt( crc1 );
                Pr( "\n", 0L, 0L );
            }
            return False;
        }
    }

    /* link and init me                                                    */
    info->isGapRootRelative = 0;
    res = (info->initKernel)(info);
    UpdateCopyFopyInfo();

    /* Start a new executor to run the outer function of the module
       in global context */
    ExecBegin( TLS->bottomLVars );
    res = res || (info->initLibrary)(info);
    ExecEnd(res ? STATUS_ERROR : STATUS_END);
    if ( res ) {
        Pr( "#W  init functions returned non-zero exit code\n", 0L, 0L );
    }
    RecordLoadedModule(info, CSTR_STRING(filename));

    return True;
}


/****************************************************************************
**
*F  FuncLOAD_STAT( <self>, <name>, <crc> )  . . . . try to load static module
*/
Obj FuncLOAD_STAT (
    Obj                 self,
    Obj                 filename,
    Obj                 crc )
{
    StructInitInfo *    info = 0;
    Obj                 crc1;
    Int                 k;
    Int                 res;

    /* check the argument                                                  */
    while ( ! IsStringConv( filename ) ) {
        filename = ErrorReturnObj(
            "<filename> must be a string (not a %s)",
            (Int)TNAM_OBJ(filename), 0L,
            "you can replace <filename> via 'return <filename>;'" );
    }
    while ( !IS_INTOBJ(crc) && crc!=False ) {
        crc = ErrorReturnObj(
            "<crc> must be a small integer or 'false' (not a %s)",
            (Int)TNAM_OBJ(crc), 0L,
            "you can replace <crc> via 'return <crc>;'" );
    }

    /* try to find the module                                              */
    for ( k = 0;  CompInitFuncs[k];  k++ ) {
        info = (*(CompInitFuncs[k]))();
        if ( info == 0 ) {
            continue;
        }
        if ( ! SyStrcmp( CSTR_STRING(filename), info->name ) ) {
            break;
        }
    }
    if ( CompInitFuncs[k] == 0 ) {
        if ( SyDebugLoading ) {
            Pr( "#I  LOAD_STAT: no module named '%s' found\n",
                (Int)CSTR_STRING(filename), 0L );
        }
        return False;
    }

    /* check the crc value                                                 */
    if ( crc != False ) {
        crc1 = INTOBJ_INT( info->crc );
        if ( ! EQ( crc, crc1 ) ) {
            if ( SyDebugLoading ) {
                Pr( "#I  LOAD_STAT: crc values do not match, gap ", 0L, 0L );
                PrintInt( crc );
                Pr( ", stat ", 0L, 0L );
                PrintInt( crc1 );
                Pr( "\n", 0L, 0L );
            }
            return False;
        }
    }

    /* link and init me                                                    */
    info->isGapRootRelative = 0;
    res = (info->initKernel)(info);
    UpdateCopyFopyInfo();
    /* Start a new executor to run the outer function of the module
       in global context */
    ExecBegin( TLS->bottomLVars );
    res = res || (info->initLibrary)(info);
    ExecEnd(res ? STATUS_ERROR : STATUS_END);
    if ( res ) {
        Pr( "#W  init functions returned non-zero exit code\n", 0L, 0L );
    }
    RecordLoadedModule(info, CSTR_STRING(filename));

    return True;
}


/****************************************************************************
**
*F  FuncSHOW_STAT() . . . . . . . . . . . . . . . . . . . show static modules
*/
Obj FuncSHOW_STAT (
    Obj                 self )
{
    Obj                 modules;
    Obj                 name;
    StructInitInfo *    info;
    Int                 k;
    Int                 im;
    Int                 len;

    /* count the number of install modules                                 */
    for ( k = 0,  im = 0;  CompInitFuncs[k];  k++ ) {
        info = (*(CompInitFuncs[k]))();
        if ( info == 0 ) {
            continue;
        }
        im++;
    }

    /* make a list of modules with crc values                              */
    modules = NEW_PLIST( T_PLIST, 2*im );
    SET_LEN_PLIST( modules, 2*im );

    for ( k = 0,  im = 1;  CompInitFuncs[k];  k++ ) {
        info = (*(CompInitFuncs[k]))();
        if ( info == 0 ) {
            continue;
        }
        /*CCC name = NEW_STRING( SyStrlen(info->name) );
          SyStrncat( CSTR_STRING(name), info->name, SyStrlen(info->name) );CCC*/
        len = SyStrlen(info->name);
        C_NEW_STRING(name, len, info->name);

        SET_ELM_PLIST( modules, im, name );

        /* compute the crc value                                           */
        SET_ELM_PLIST( modules, im+1, INTOBJ_INT( info->crc ) );
        im += 2;
    }

    return modules;
}


/****************************************************************************
**
*F  FuncLoadedModules( <self> ) . . . . . . . . . . . list all loaded modules
*/
Obj FuncLoadedModules (
    Obj                 self )
{
    Int                 i;
    StructInitInfo *    m;
    Obj                 str;
    Obj                 list;

    /* create a list                                                       */
    list = NEW_PLIST( T_PLIST, NrModules * 3 );
    SET_LEN_PLIST( list, NrModules * 3 );
    for ( i = 0;  i < NrModules;  i++ ) {
        m = Modules[i];
        if ( m->type == MODULE_BUILTIN ) {
            SET_ELM_PLIST( list, 3*i+1, ObjsChar[(Int)'b'] );
            CHANGED_BAG(list);
            C_NEW_STRING( str, SyStrlen(m->name), m->name );
            SET_ELM_PLIST( list, 3*i+2, str );
            SET_ELM_PLIST( list, 3*i+3, INTOBJ_INT(m->version) );
        }
        else if ( m->type == MODULE_DYNAMIC ) {
            SET_ELM_PLIST( list, 3*i+1, ObjsChar[(Int)'d'] );
            CHANGED_BAG(list);
            C_NEW_STRING( str, SyStrlen(m->name), m->name );
            SET_ELM_PLIST( list, 3*i+2, str );
            CHANGED_BAG(list);
            C_NEW_STRING( str, SyStrlen(m->filename), m->filename );
            SET_ELM_PLIST( list, 3*i+3, str );
        }
        else if ( m->type == MODULE_STATIC ) {
            SET_ELM_PLIST( list, 3*i+1, ObjsChar[(Int)'s'] );
            CHANGED_BAG(list);
            C_NEW_STRING( str, SyStrlen(m->name), m->name );
            SET_ELM_PLIST( list, 3*i+2, str );
            CHANGED_BAG(list);
            C_NEW_STRING( str, SyStrlen(m->filename), m->filename );
            SET_ELM_PLIST( list, 3*i+3, str );
        }
    }
    return CopyObj( list, 0 );
}


/****************************************************************************
**


*F * * * * * * * * * * * * * * debug functions  * * * * * * * * * * * * * * *
*/

/****************************************************************************
**

*F  FuncGASMAN( <self>, <args> )  . . . . . . . . .  expert function 'GASMAN'
**
**  'FuncGASMAN' implements the internal function 'GASMAN'
**
**  'GASMAN( "display" | "clear" | "collect" | "message" | "partial" )'
*/
Obj FuncGASMAN (
    Obj                 self,
    Obj                 args )
{
    Obj                 cmd;            /* argument                        */
    UInt                i,  k;          /* loop variables                  */
    Char                buf[100];

    /* check the argument                                                  */
    while ( ! IS_SMALL_LIST(args) || LEN_LIST(args) == 0 ) {
        args = ErrorReturnObj(
            "usage: GASMAN( \"display\"|\"displayshort\"|\"clear\"|\"collect\"|\"message\"|\"partial\" )",
            0L, 0L,
            "you can replace the argument list <args> via 'return <args>;'" );
    }

    /* loop over the arguments                                             */
    for ( i = 1; i <= LEN_LIST(args); i++ ) {

        /* evaluate and check the command                                  */
        cmd = ELM_PLIST( args, i );
again:
        while ( ! IsStringConv(cmd) ) {
           cmd = ErrorReturnObj(
               "GASMAN: <cmd> must be a string (not a %s)",
               (Int)TNAM_OBJ(cmd), 0L,
               "you can replace <cmd> via 'return <cmd>;'" );
       }

        /* if request display the statistics                               */
        if ( SyStrcmp( CSTR_STRING(cmd), "display" ) == 0 ) {
            Pr( "%40s ", (Int)"type",  0L          );
            Pr( "%8s %8s ",  (Int)"alive", (Int)"kbyte" );
            Pr( "%8s %8s\n",  (Int)"total", (Int)"kbyte" );
            for ( k = 0; k < 256; k++ ) {
                if ( InfoBags[k].name != 0 ) {
                    buf[0] = '\0';
                    SyStrncat( buf, InfoBags[k].name, 40 );
                    Pr("%40s ",    (Int)buf, 0L );
                    Pr("%8d %8d ", (Int)InfoBags[k].nrLive,
                                   (Int)(InfoBags[k].sizeLive/1024));
                    Pr("%8d %8d\n",(Int)InfoBags[k].nrAll,
                                   (Int)(InfoBags[k].sizeAll/1024));
                }
            }
        }

        /* if request give a short display of the statistics                */
        else if ( SyStrcmp( CSTR_STRING(cmd), "displayshort" ) == 0 ) {
            Pr( "%40s ", (Int)"type",  0L          );
            Pr( "%8s %8s ",  (Int)"alive", (Int)"kbyte" );
            Pr( "%8s %8s\n",  (Int)"total", (Int)"kbyte" );
            for ( k = 0; k < 256; k++ ) {
                if ( InfoBags[k].name != 0 && 
                     (InfoBags[k].nrLive != 0 ||
                      InfoBags[k].sizeLive != 0 ||
                      InfoBags[k].nrAll != 0 ||
                      InfoBags[k].sizeAll != 0) ) {
                    buf[0] = '\0';
                    SyStrncat( buf, InfoBags[k].name, 40 );
                    Pr("%40s ",    (Int)buf, 0L );
                    Pr("%8d %8d ", (Int)InfoBags[k].nrLive,
                                   (Int)(InfoBags[k].sizeLive/1024));
                    Pr("%8d %8d\n",(Int)InfoBags[k].nrAll,
                                   (Int)(InfoBags[k].sizeAll/1024));
                }
            }
        }

        /* if request display the statistics                               */
        else if ( SyStrcmp( CSTR_STRING(cmd), "clear" ) == 0 ) {
            for ( k = 0; k < 256; k++ ) {
#ifdef GASMAN_CLEAR_TO_LIVE
                InfoBags[k].nrAll    = InfoBags[k].nrLive;
                InfoBags[k].sizeAll  = InfoBags[k].sizeLive;
#else
                InfoBags[k].nrAll    = 0;
                InfoBags[k].sizeAll  = 0;
#endif
            }
        }

        /* or collect the garbage                                          */
        else if ( SyStrcmp( CSTR_STRING(cmd), "collect" ) == 0 ) {
            CollectBags(0,1);
        }

        /* or collect the garbage                                          */
        else if ( SyStrcmp( CSTR_STRING(cmd), "partial" ) == 0 ) {
            CollectBags(0,0);
        }

        /* or display information about global bags                        */
        else if ( SyStrcmp( CSTR_STRING(cmd), "global" ) == 0 ) {
            for ( i = 0;  i < GlobalBags.nr;  i++ ) {
                if ( *(GlobalBags.addr[i]) != 0 ) {
                    Pr( "%50s: %12d bytes\n", (Int)GlobalBags.cookie[i], 
                        (Int)SIZE_BAG(*(GlobalBags.addr[i])) );
                }
            }
        }

        /* or finally toggle Gasman messages                               */
        else if ( SyStrcmp( CSTR_STRING(cmd), "message" ) == 0 ) {
            SyMsgsFlagBags = (SyMsgsFlagBags + 1) % 3;
        }

        /* otherwise complain                                              */
        else {
            cmd = ErrorReturnObj(
                "GASMAN: <cmd> must be %s or %s",
                (Int)"\"display\" or \"clear\" or \"global\" or ",
                (Int)"\"collect\" or \"partial\" or \"message\"",
                "you can replace <cmd> via 'return <cmd>;'" );
            goto again;
        }
    }

    /* return nothing, this function is a procedure                        */
    return 0;
}

Obj FuncGASMAN_STATS(Obj self)
{
  Obj res;
  Obj row;
  Obj entry;
  UInt i,j;
  Int x;
  res = NEW_PLIST(T_PLIST_TAB_RECT + IMMUTABLE, 2);
  SET_LEN_PLIST(res, 2);
  for (i = 1; i <= 2; i++)
    {
      row = NEW_PLIST(T_PLIST_CYC + IMMUTABLE, 9);
      SET_ELM_PLIST(res, i, row);
      CHANGED_BAG(res);
      SET_LEN_PLIST(row, 9);
      for (j = 1; j <= 8; j++)
        {
          x = SyGasmanNumbers[i-1][j];

          /* convert x to GAP integer. x may be too big to be a small int */
          if (x < (1L << NR_SMALL_INT_BITS))
            entry = INTOBJ_INT(x);
          else
            entry = SUM( PROD(INTOBJ_INT(x >> (NR_SMALL_INT_BITS/2)),
                              INTOBJ_INT(1 << (NR_SMALL_INT_BITS/2))),
                         INTOBJ_INT( x % ( 1 << (NR_SMALL_INT_BITS/2))));
          SET_ELM_PLIST(row, j, entry);
        }
      SET_ELM_PLIST(row, 9, INTOBJ_INT(SyGasmanNumbers[i-1][0]));       
    }
  return res;      
}

Obj FuncGASMAN_MESSAGE_STATUS( Obj self )
{
  return INTOBJ_INT(SyMsgsFlagBags);
}

Obj FuncGASMAN_LIMITS( Obj self )
{
  Obj list;
  list = NEW_PLIST(T_PLIST_CYC+IMMUTABLE, 3);
  SET_LEN_PLIST(list,3);
  SET_ELM_PLIST(list, 1, INTOBJ_INT(SyStorMin));
  SET_ELM_PLIST(list, 2, INTOBJ_INT(SyStorMax));
  SET_ELM_PLIST(list, 3, INTOBJ_INT(SyStorKill));
  return list;
}

/****************************************************************************
**
*F  FuncSHALLOW_SIZE( <self>, <obj> ) . . . .  expert function 'SHALLOW_SIZE'
*/
Obj FuncSHALLOW_SIZE (
    Obj                 self,
    Obj                 obj )
{
  if (IS_INTOBJ(obj) || IS_FFE(obj))
    return INTOBJ_INT(0);
  else
    return ObjInt_UInt( SIZE_BAG( obj ) );
}


/****************************************************************************
**
*F  FuncTNUM_OBJ( <self>, <obj> ) . . . . . . . .  expert function 'TNUM_OBJ'
*/

Obj FuncTNUM_OBJ (
    Obj                 self,
    Obj                 obj )
{
    Obj                 res;
    Obj                 str;
    Int                 len;
    const Char *        cst;

    res = NEW_PLIST( T_PLIST, 2 );
    SET_LEN_PLIST( res, 2 );

    /* set the type                                                        */
    SET_ELM_PLIST( res, 1, INTOBJ_INT( TNUM_OBJ(obj) ) );
    cst = TNAM_OBJ(obj);
    /*CCC    str = NEW_STRING( SyStrlen(cst) );
      SyStrncat( CSTR_STRING(str), cst, SyStrlen(cst) );CCC*/
    len = SyStrlen(cst);
    C_NEW_STRING(str, len, cst);
    SET_ELM_PLIST( res, 2, str );

    /* and return                                                          */
    return res;
}

Obj FuncTNUM_OBJ_INT (
    Obj                 self,
    Obj                 obj )
{

  
    return  INTOBJ_INT( TNUM_OBJ(obj) ) ;
}

/****************************************************************************
**
*F  FuncXTNUM_OBJ( <self>, <obj> )  . . . . . . . expert function 'XTNUM_OBJ'
*/
Obj FuncXTNUM_OBJ (
    Obj                 self,
    Obj                 obj )
{
    Obj                 res;
    Obj                 str;

    res = NEW_PLIST( T_PLIST, 2 );
    SET_LEN_PLIST( res, 2 );
    SET_ELM_PLIST( res, 1, Fail );
    C_NEW_STRING(str, 16, "xtnums abolished");
    SET_ELM_PLIST(res, 2,str);
    /* and return                                                          */
    return res;
}


/****************************************************************************
**
*F  FuncOBJ_HANDLE( <self>, <obj> ) . . . . . .  expert function 'OBJ_HANDLE'
*/
Obj FuncOBJ_HANDLE (
    Obj                 self,
    Obj                 obj )
{
    UInt                hand;
    UInt                prod;
    Obj                 rem;

    if ( IS_INTOBJ(obj) ) {
        return (Obj)INT_INTOBJ(obj);
    }
    else if ( TNUM_OBJ(obj) == T_INTPOS ) {
        hand = 0;
        prod = 1;
        while ( EQ( obj, INTOBJ_INT(0) ) == 0 ) {
            rem  = RemInt( obj, INTOBJ_INT( 1 << 16 ) );
            obj  = QuoInt( obj, INTOBJ_INT( 1 << 16 ) );
            hand = hand + prod * INT_INTOBJ(rem);
            prod = prod * ( 1 << 16 );
        }
        return (Obj) hand;
    }
    else {
        ErrorQuit( "<handle> must be a positive integer", 0L, 0L );
        return (Obj) 0;
    }
}


/****************************************************************************
**
*F  FuncHANDLE_OBJ( <self>, <obj> ) . . . . . .  expert function 'HANDLE_OBJ'
*/
Obj FuncHANDLE_OBJ (
    Obj                 self,
    Obj                 obj )
{
    Obj                 hnum;
    Obj                 prod;
    Obj                 tmp;
    UInt                hand;

    hand = (UInt) obj;
    hnum = INTOBJ_INT(0);
    prod = INTOBJ_INT(1);
    while ( 0 < hand ) {
        tmp  = PROD( prod, INTOBJ_INT( hand & 0xffff ) );
        prod = PROD( prod, INTOBJ_INT( 1 << 16 ) );
        hnum = SUM(  hnum, tmp );
        hand = hand >> 16;
    }
    return hnum;
}

Obj FuncMASTER_POINTER_NUMBER(Obj self, Obj o)
{
    if ((void **) o >= (void **) MptrBags && (void **) o < (void **) OldBags) {
        return INTOBJ_INT( ((void **) o - (void **) MptrBags) + 1 );
    } else {
        return INTOBJ_INT( 0 );
    }
}

Obj FuncFUNC_BODY_SIZE(Obj self, Obj f)
{
    Obj body;
    if (TNUM_OBJ(f) != T_FUNCTION) return Fail;
    body = BODY_FUNC(f);
    if (body == 0) return INTOBJ_INT(0);
    else return INTOBJ_INT( SIZE_BAG( body ) );
}

/****************************************************************************
**
*F  FuncSWAP_MPTR( <self>, <obj1>, <obj2> ) . . . . . . . swap master pointer
**
**  Never use this function unless you are debugging.
*/
Obj FuncSWAP_MPTR (
    Obj                 self,
    Obj                 obj1,
    Obj                 obj2 )
{
    if ( TNUM_OBJ(obj1) == T_INT || TNUM_OBJ(obj1) == T_FFE ) {
        ErrorQuit("SWAP_MPTR: <obj1> must not be an integer or ffe", 0L, 0L);
        return 0;
    }
    if ( TNUM_OBJ(obj2) == T_INT || TNUM_OBJ(obj2) == T_FFE ) {
        ErrorQuit("SWAP_MPTR: <obj2> must not be an integer or ffe", 0L, 0L);
        return 0;
    }
        
    SwapMasterPoint( obj1, obj2 );
    return 0;
}


/****************************************************************************
**

*F * * * * * * * * * * * * * initialize package * * * * * * * * * * * * * * *
*/


/****************************************************************************
**

*F  FillInVersion( <module>, <rev_c>, <rev_h> ) . . .  fill in version number
*/
void FillInVersion (
    StructInitInfo *            module )
{
}


/****************************************************************************
**
*F  RequireModule( <calling>, <required>, <version> ) . . . .  require module
*/
void RequireModule (
    StructInitInfo *            module,
    const Char *                required,
    UInt                        version )
{
}


/****************************************************************************
**
*F  InitBagNamesFromTable( <table> )  . . . . . . . . .  initialise bag names
*/
void InitBagNamesFromTable (
    StructBagNames *            tab )
{
    Int                         i;

    for ( i = 0;  tab[i].tnum != -1;  i++ ) {
        InfoBags[tab[i].tnum].name = tab[i].name;
    }
}


/****************************************************************************
**
*F  InitClearFiltsTNumsFromTable( <tab> ) . . .  initialise clear filts tnums
*/
void InitClearFiltsTNumsFromTable (
    Int *               tab )
{
    Int                 i;

    for ( i = 0;  tab[i] != -1;  i += 2 ) {
        ClearFiltsTNums[tab[i]] = tab[i+1];
    }
}


/****************************************************************************
**
*F  InitHasFiltListTNumsFromTable( <tab> )  . . initialise tester filts tnums
*/
void InitHasFiltListTNumsFromTable (
    Int *               tab )
{
    Int                 i;

    for ( i = 0;  tab[i] != -1;  i += 3 ) {
        HasFiltListTNums[tab[i]][tab[i+1]] = tab[i+2];
    }
}


/****************************************************************************
**
*F  InitSetFiltListTNumsFromTable( <tab> )  . . initialise setter filts tnums
*/
void InitSetFiltListTNumsFromTable (
    Int *               tab )
{
    Int                 i;

    for ( i = 0;  tab[i] != -1;  i += 3 ) {
        SetFiltListTNums[tab[i]][tab[i+1]] = tab[i+2];
    }
}


/****************************************************************************
**
*F  InitResetFiltListTNumsFromTable( <tab> )  initialise unsetter filts tnums
*/
void InitResetFiltListTNumsFromTable (
    Int *               tab )
{
    Int                 i;

    for ( i = 0;  tab[i] != -1;  i += 3 ) {
        ResetFiltListTNums[tab[i]][tab[i+1]] = tab[i+2];
    }
}


/****************************************************************************
**
*F  InitGVarFiltsFromTable( <tab> ) . . . . . . . . . . . . . . . new filters
*/
void InitGVarFiltsFromTable (
    StructGVarFilt *    tab )
{
    Int                 i;

    for ( i = 0;  tab[i].name != 0;  i++ ) {
        AssGVar( GVarName( tab[i].name ),
            NewFilterC( tab[i].name, 1, tab[i].argument, tab[i].handler ) );
        MakeReadOnlyGVar( GVarName( tab[i].name ) );
    }
}


/****************************************************************************
**
*F  InitGVarAttrsFromTable( <tab> ) . . . . . . . . . . . . .  new attributes
*/
void InitGVarAttrsFromTable (
    StructGVarAttr *    tab )
{
    Int                 i;

    for ( i = 0;  tab[i].name != 0;  i++ ) {
       AssGVar( GVarName( tab[i].name ),
         NewAttributeC( tab[i].name, 1, tab[i].argument, tab[i].handler ) );
       MakeReadOnlyGVar( GVarName( tab[i].name ) );
    }
}


/****************************************************************************
**
*F  InitGVarPropsFromTable( <tab> ) . . . . . . . . . . . . .  new properties
*/
void InitGVarPropsFromTable (
    StructGVarProp *    tab )
{
    Int                 i;

    for ( i = 0;  tab[i].name != 0;  i++ ) {
       AssGVar( GVarName( tab[i].name ),
         NewPropertyC( tab[i].name, 1, tab[i].argument, tab[i].handler ) );
       MakeReadOnlyGVar( GVarName( tab[i].name ) );
    }
}


/****************************************************************************
**
*F  InitGVarOpersFromTable( <tab> ) . . . . . . . . . . . . .  new operations
*/
void InitGVarOpersFromTable (
    StructGVarOper *    tab )
{
    Int                 i;

    for ( i = 0;  tab[i].name != 0;  i++ ) {
        AssGVar( GVarName( tab[i].name ), NewOperationC( tab[i].name, 
            tab[i].nargs, tab[i].args, tab[i].handler ) );
        MakeReadOnlyGVar( GVarName( tab[i].name ) );
    }
}


/****************************************************************************
**
*F  InitGVarFuncsFromTable( <tab> ) . . . . . . . . . . . . . . new functions
*/
void InitGVarFuncsFromTable (
    StructGVarFunc *    tab )
{
    Int                 i;

    for ( i = 0;  tab[i].name != 0;  i++ ) {
        AssGVar( GVarName( tab[i].name ), NewFunctionC( tab[i].name, 
            tab[i].nargs, tab[i].args, tab[i].handler ) );
        MakeReadOnlyGVar( GVarName( tab[i].name ) );
    }
}


/****************************************************************************
**
*F  InitHdlrFiltsFromTable( <tab> ) . . . . . . . . . . . . . . . new filters
*/
void InitHdlrFiltsFromTable (
    StructGVarFilt *    tab )
{
    Int                 i;

    for ( i = 0;  tab[i].name != 0;  i++ ) {
        InitHandlerFunc( tab[i].handler, tab[i].cookie );
        InitFopyGVar( tab[i].name, tab[i].filter );
    }
}


/****************************************************************************
**
*F  InitHdlrAttrsFromTable( <tab> ) . . . . . . . . . . . . .  new attributes
*/
void InitHdlrAttrsFromTable (
    StructGVarAttr *    tab )
{
    Int                 i;

    for ( i = 0;  tab[i].name != 0;  i++ ) {
        InitHandlerFunc( tab[i].handler, tab[i].cookie );
        InitFopyGVar( tab[i].name, tab[i].attribute );
    }
}


/****************************************************************************
**
*F  InitHdlrPropsFromTable( <tab> ) . . . . . . . . . . . . .  new properties
*/
void InitHdlrPropsFromTable (
    StructGVarProp *    tab )
{
    Int                 i;

    for ( i = 0;  tab[i].name != 0;  i++ ) {
        InitHandlerFunc( tab[i].handler, tab[i].cookie );
        InitFopyGVar( tab[i].name, tab[i].property );
    }
}


/****************************************************************************
**
*F  InitHdlrOpersFromTable( <tab> ) . . . . . . . . . . . . .  new operations
*/
void InitHdlrOpersFromTable (
    StructGVarOper *    tab )
{
    Int                 i;

    for ( i = 0;  tab[i].name != 0;  i++ ) {
        InitHandlerFunc( tab[i].handler, tab[i].cookie );
        InitFopyGVar( tab[i].name, tab[i].operation );
    }
}


/****************************************************************************
**
*F  InitHdlrFuncsFromTable( <tab> ) . . . . . . . . . . . . . . new functions
*/
void InitHdlrFuncsFromTable (
    StructGVarFunc *    tab )
{
    Int                 i;

    for ( i = 0;  tab[i].name != 0;  i++ ) {
        InitHandlerFunc( tab[i].handler, tab[i].cookie );
    }
}


/****************************************************************************
**
*F  ImportGVarFromLibrary( <name>, <address> )  . . .  import global variable
*/


void ImportGVarFromLibrary(
    const Char *        name,
    Obj *               address )
{
    if ( NrImportedGVars == 1024 ) {
        Pr( "#W  warning: too many imported GVars\n", 0L, 0L );
    }
    else {
        ImportedGVars[NrImportedGVars].name    = name;
        ImportedGVars[NrImportedGVars].address = address;
        NrImportedGVars++;
    }
    if ( address != 0 ) {
        InitCopyGVar( name, address );
    }
}


/****************************************************************************
**
*F  ImportFuncFromLibrary( <name>, <address> )  . . .  import global function
*/


void ImportFuncFromLibrary(
    const Char *        name,
    Obj *               address )
{
    if ( NrImportedFuncs == 1024 ) {
        Pr( "#W  warning: too many imported Funcs\n", 0L, 0L );
    }
    else {
        ImportedFuncs[NrImportedFuncs].name    = name;
        ImportedFuncs[NrImportedFuncs].address = address;
        NrImportedFuncs++;
    }
    if ( address != 0 ) {
        InitFopyGVar( name, address );
    }
}


/****************************************************************************
**
*F  FuncExportToKernelFinished( <self> )  . . . . . . . . . . check functions
*/
Obj FuncExportToKernelFinished (
    Obj             self )
{
    UInt            i;
    Int             errs = 0;
    Obj             val;

    SyInitializing = 0;
    for ( i = 0;  i < NrImportedGVars;  i++ ) {
        if ( ImportedGVars[i].address == 0 ) {
            val = ValAutoGVar(GVarName(ImportedGVars[i].name));
            if ( val == 0 ) {
                errs++;
                if ( ! SyQuiet ) {
                    Pr( "#W  global variable '%s' has not been defined\n",
                        (Int)ImportedFuncs[i].name, 0L );
                }
            }
        }
        else if ( *ImportedGVars[i].address == 0 ) {
            errs++;
            if ( ! SyQuiet ) {
                Pr( "#W  global variable '%s' has not been defined\n",
                    (Int)ImportedGVars[i].name, 0L );
            }
        }
        else {
            MakeReadOnlyGVar(GVarName(ImportedGVars[i].name));
        }
    }
    
    for ( i = 0;  i < NrImportedFuncs;  i++ ) {
        if (  ImportedFuncs[i].address == 0 ) {
            val = ValAutoGVar(GVarName(ImportedFuncs[i].name));
            if ( val == 0 || ! IS_FUNC(val) ) {
                errs++;
                if ( ! SyQuiet ) {
                    Pr( "#W  global function '%s' has not been defined\n",
                        (Int)ImportedFuncs[i].name, 0L );
                }
            }
        }
        else if ( *ImportedFuncs[i].address == ErrorMustEvalToFuncFunc
          || *ImportedFuncs[i].address == ErrorMustHaveAssObjFunc )
        {
            errs++;
            if ( ! SyQuiet ) {
                Pr( "#W  global function '%s' has not been defined\n",
                    (Int)ImportedFuncs[i].name, 0L );
            }
        }
        else {
            MakeReadOnlyGVar(GVarName(ImportedFuncs[i].name));
        }
    }
    
    return errs == 0 ? True : False;
}


/****************************************************************************
**
*F  FuncSleep( <self>, <secs> )
**
*/

Obj FuncSleep( Obj self, Obj secs )
{
  Int  s;

  while( ! IS_INTOBJ(secs) )
    secs = ErrorReturnObj( "<secs> must be a small integer", 0L, 0L, 
                           "you can replace <secs> via 'return <secs>;'" );

  
  if ( (s = INT_INTOBJ(secs)) > 0)
    SySleep((UInt)s);
  
  /* either we used up the time, or we were interrupted. */
  if (SyIsIntr())
    {
      ClearError(); /* The interrupt may still be pending */
      ErrorReturnVoid("user interrupt in sleep", 0L, 0L,
                    "you can 'return;' as if the sleep was finished");
    }
  
  return (Obj) 0;
}

/****************************************************************************
**
*F  FuncGAP_EXIT_CODE() . . . . . . . . Set the code with which GAP exits.
**
*/

Obj FuncGAP_EXIT_CODE( Obj self, Obj code )
{
  if (code == False || code == Fail)
    SystemErrorCode = 1;
  else if (code == True)
    SystemErrorCode = 0;
  else if (IS_INTOBJ(code))
    SystemErrorCode = INT_INTOBJ(code);
  else
    ErrorQuit("GAP_EXIT_CODE: Argument must be boolean or integer", 0L, 0L);
  return (Obj) 0;
}


/****************************************************************************
**
*F  FuncQUIT_GAP()
**
*/

Obj FuncQUIT_GAP( Obj self )
{
  TLS->UserHasQUIT = 1;
  ReadEvalError();
  return (Obj)0; 
}

/****************************************************************************
**
*F  FuncFORCE_QUIT_GAP()
**
*/

Obj FuncFORCE_QUIT_GAP( Obj self )
{
  SyExit(0);
  return (Obj) 0; /* should never get here */
}



/****************************************************************************
**
*F  KERNEL_INFO() ......................record of information from the kernel
** 
** The general idea is to put all kernel-specific info in here, and clean up
** the assortment of global variables previously used
*/

Obj FuncKERNEL_INFO(Obj self) {
  Obj res = NEW_PREC(0);
  UInt r,len,lenvec,lenstr;
  Char *p;
  Obj tmp,list,str;
  UInt i,j;
  extern UInt SyNumProcessors;

  /* GAP_ARCHITECTURE                                                    */
  tmp = NEW_STRING(SyStrlen(SyArchitecture));
  RetypeBag( tmp, IMMUTABLE_TNUM(TNUM_OBJ(tmp)) );
  SyStrncat( CSTR_STRING(tmp), SyArchitecture, SyStrlen(SyArchitecture) );
  r = RNamName("GAP_ARCHITECTURE");
  AssPRec(res,r,tmp);
  /* KERNEL_VERSION */
  tmp = NEW_STRING(SyStrlen(SyKernelVersion));
  RetypeBag( tmp, IMMUTABLE_TNUM(TNUM_OBJ(tmp)) );
  SyStrncat( CSTR_STRING(tmp), SyKernelVersion, SyStrlen(SyKernelVersion) );
  r = RNamName("KERNEL_VERSION");
  AssPRec(res,r,tmp);
  /* GAP_ROOT_PATH                                                       */
  /* do we need this. Could we rebuild it from the command line in GAP
     if so, should we                                                    */
  list = NEW_PLIST( T_PLIST+IMMUTABLE, MAX_GAP_DIRS );
  for ( i = 0, j = 1;  i < MAX_GAP_DIRS;  i++ ) {
    if ( SyGapRootPaths[i][0] ) {
      len = SyStrlen(SyGapRootPaths[i]);
      tmp = NEW_STRING(len);
      RetypeBag( tmp, IMMUTABLE_TNUM(TNUM_OBJ(tmp)) );
      SyStrncat( CSTR_STRING(tmp), SyGapRootPaths[i], len );
      SET_ELM_PLIST( list, j, tmp );
      j++;
    }
  }
  SET_LEN_PLIST( list, j-1 );
  r = RNamName("GAP_ROOT_PATHS");
  AssPRec(res,r,list);
      
    
    /* make command line and environment available to GAP level       */
    for (lenvec=0; SyOriginalArgv[lenvec]; lenvec++);
    tmp = NEW_PLIST( T_PLIST+IMMUTABLE, lenvec );
    SET_LEN_PLIST( tmp, lenvec );
    for (i = 0; i<lenvec; i++) {
      lenstr = SyStrlen(SyOriginalArgv[i]);
      str = NEW_STRING(lenstr);
      SyStrncat(CSTR_STRING(str), SyOriginalArgv[i], lenstr);
      SET_LEN_STRING(str, lenstr);
      SET_ELM_PLIST(tmp, i+1, str);
      CHANGED_BAG(tmp);
    }
    r = RNamName("COMMAND_LINE");
    AssPRec(res,r, tmp);

    tmp = NEW_PREC(0);
    for (i = 0; sysenviron[i]; i++) {
      for (p = sysenviron[i]; *p != '='; p++)
        ;
      *p++ = '\0';
      lenstr = SyStrlen(p);
      str = NEW_STRING(lenstr);
      SyStrncat(CSTR_STRING(str),p, lenstr);
      SET_LEN_STRING(str, lenstr);
      AssPRec(tmp,RNamName(sysenviron[i]), str);
      *--p = '='; /* change back to allow a convenient rerun */
    }
    r = RNamName("ENVIRONMENT");
    AssPRec(res,r, tmp);

#ifdef CONFIGNAME
    /* and also the CONFIGNAME of the running  GAP kernel  */
    p = CONFIGNAME;
    lenstr = SyStrlen(p);
    str = NEW_STRING(lenstr);
    SyStrncat(CSTR_STRING(str), p, lenstr);
    SET_LEN_STRING(str, lenstr);
    r = RNamName("CONFIGNAME");
    AssPRec(res, r, str);
#endif

    r = RNamName("NUM_CPUS");
    AssPRec(res, r, INTOBJ_INT(SyNumProcessors));
   
    return res;
  
}

/****************************************************************************
**
*F FuncTHREAD_UI  ... Whether we use a multi-threaded interface
**
*/

Obj FuncTHREAD_UI(Obj self)
{
  extern UInt ThreadUI;
  return ThreadUI ? True : False;
}


/****************************************************************************
**
*F FuncGETPID  ... export UNIX getpid to GAP level
**
*/

Obj FuncGETPID(Obj self) {
  return INTOBJ_INT(getpid());
}

GVarDescriptor GVarTHREAD_INIT;
GVarDescriptor GVarTHREAD_EXIT;

void ThreadedInterpreter(void *funcargs) {
  Obj tmp, func;
  int i;

  /* intialize everything and begin an interpreter                       */
  TLS->stackNams   = NEW_PLIST( T_PLIST, 16 );
  TLS->countNams   = 0;
  TLS->readTop     = 0;
  TLS->readTilde   = 0;
  TLS->currLHSGVar = 0;
  TLS->intrCoding = 0;
  TLS->intrIgnoring = 0;
  TLS->nrError = 0;
  TLS->thrownObject = 0;
  TLS->bottomLVars = NewBag( T_HVARS, 3*sizeof(Obj) );
  tmp = NewFunctionC( "bottom", 0, "", 0 );
  PTR_BAG(TLS->bottomLVars)[0] = tmp;
  tmp = NewBag( T_BODY, NUMBER_HEADER_ITEMS_BODY*sizeof(Obj) );
  BODY_FUNC( PTR_BAG(TLS->bottomLVars)[0] ) = tmp;
  TLS->currLVars = TLS->bottomLVars;

  IntrBegin( TLS->bottomLVars );
  tmp = KEPTALIVE(funcargs);
  StopKeepAlive(funcargs);
  func = ELM_PLIST(tmp, 1);
  for (i=2; i<=LEN_PLIST(tmp); i++)
  {
    Obj item = ELM_PLIST(tmp, i);
    SET_ELM_PLIST(tmp, i-1, item);
  }
  SET_LEN_PLIST(tmp, LEN_PLIST(tmp)-1);

  if (!READ_ERROR()) {
    Obj init, exit;
    if (sySetjmp(TLS->threadExit))
      return;
    init = GVarOptFunc(&GVarTHREAD_INIT);
    if (init) CALL_0ARGS(init);
    FuncCALL_FUNC_LIST((Obj) 0, func, tmp);
    exit = GVarOptFunc(&GVarTHREAD_EXIT);
    if (exit) CALL_0ARGS(exit);
    PushVoidObj();
    /* end the interpreter                                                 */
    IntrEnd( 0UL );
  } else {
    IntrEnd( 1UL );
    ClearError();
  } 
}

UInt BreakPointValue;

Obj BREAKPOINT(Obj self, Obj arg) {
  if (IS_INTOBJ(arg))
    BreakPointValue = INT_INTOBJ(arg);
  return (Obj) 0;
}


/****************************************************************************
**
*V  GVarFuncs . . . . . . . . . . . . . . . . . . list of functions to export
*/
static StructGVarFunc GVarFuncs [] = {

    { "Runtime", 0, "",
      FuncRuntime, "src/gap.c:Runtime" },

    { "RUNTIMES", 0, "",
      FuncRUNTIMES, "src/gap.c:RUNTIMES" },

    { "SizeScreen", -1, "args",
      FuncSizeScreen, "src/gap.c:SizeScreen" },

    { "ID_FUNC", 1, "object",
      FuncID_FUNC, "src/gap.c:ID_FUNC" },

    { "RESTART_GAP", 1, "cmdline",
      FuncRESTART_GAP, "src/gap.c:RESTART_GAP" },

    { "ExportToKernelFinished", 0, "",
      FuncExportToKernelFinished, "src/gap.c:ExportToKernelFinished" },

    { "DownEnv", -1, "args",
      FuncDownEnv, "src/gap.c:DownEnv" },

    { "UpEnv", -1, "args",
      FuncUpEnv, "src/gap.c:UpEnv" },

    { "GAP_CRC", 1, "filename",
      FuncGAP_CRC, "src/gap.c:GAP_CRC" },

    { "LOAD_DYN", 2, "filename, crc",
      FuncLOAD_DYN, "src/gap.c:LOAD_DYN" },

    { "LOAD_STAT", 2, "filename, crc",
      FuncLOAD_STAT, "src/gap.c:LOAD_STAT" },

    { "SHOW_STAT", 0, "",
      FuncSHOW_STAT, "src/gap.c:SHOW_STAT" },

    { "GASMAN", -1, "args",
      FuncGASMAN, "src/gap.c:GASMAN" },

    { "GASMAN_STATS", 0, "",
      FuncGASMAN_STATS, "src/gap.c:GASMAN_STATS" },

    { "GASMAN_MESSAGE_STATUS", 0, "",
      FuncGASMAN_MESSAGE_STATUS, "src/gap.c:GASMAN_MESSAGE_STATUS" },

    { "GASMAN_LIMITS", 0, "",
      FuncGASMAN_LIMITS, "src/gap.c:GASMAN_LIMITS" },

    { "SHALLOW_SIZE", 1, "object",
      FuncSHALLOW_SIZE, "src/gap.c:SHALLOW_SIZE" },

    { "TNUM_OBJ", 1, "object",
      FuncTNUM_OBJ, "src/gap.c:TNUM_OBJ" },

    { "TNUM_OBJ_INT", 1, "object",
      FuncTNUM_OBJ_INT, "src/gap.c:TNUM_OBJ_INT" },

    { "XTNUM_OBJ", 1, "object",
      FuncXTNUM_OBJ, "src/gap.c:XTNUM_OBJ" },

    { "OBJ_HANDLE", 1, "object",
      FuncOBJ_HANDLE, "src/gap.c:OBJ_HANDLE" },

    { "HANDLE_OBJ", 1, "object",
      FuncHANDLE_OBJ, "src/gap.c:HANDLE_OBJ" },

    { "SWAP_MPTR", 2, "obj1, obj2",
      FuncSWAP_MPTR, "src/gap.c:SWAP_MPTR" },

    { "LoadedModules", 0, "",
      FuncLoadedModules, "src/gap.c:LoadedModules" },

    { "WindowCmd", 1, "arg-list",
      FuncWindowCmd, "src/gap.c:WindowCmd" },


    { "Sleep", 1, "secs",
      FuncSleep, "src/gap.c:Sleep" },

    { "GAP_EXIT_CODE", 1, "exit code",
      FuncGAP_EXIT_CODE, "src/gap.c:GAP_EXIT_CODE" },

    { "QUIT_GAP", 0, "",
      FuncQUIT_GAP, "src/gap.c:QUIT_GAP" },


    { "FORCE_QUIT_GAP", 0, "",
      FuncFORCE_QUIT_GAP, "src/gap.c:FORCE_QUIT_GAP" },


    { "SHELL", -1, "context, canReturnVoid, canReturnObj, lastDepth, setTime, prompt, promptHook, infile, outfile",
      FuncSHELL, "src/gap.c:FuncSHELL" },

    { "CALL_WITH_CATCH", 2, "func, args",
      FuncCALL_WITH_CATCH, "src/gap.c:CALL_WITH_CATCH" },

    { "JUMP_TO_CATCH", 1, "payload",
      FuncJUMP_TO_CATCH, "src/gap.c:JUMP_TO_CATCH" },


    { "KERNEL_INFO", 0, "",
      FuncKERNEL_INFO, "src/gap.c:KERNEL_INFO" },

    { "THREAD_UI", 0, "",
      FuncTHREAD_UI, "src/gap.c:THREAD_UI" },

    { "SetUserHasQuit", 1, "value",
      FuncSetUserHasQuit, "src/gap.c:SetUserHasQuit" },

    { "GETPID", 0, "",
      FuncGETPID, "src/gap.c:GETPID" },

    { "BREAKPOINT", 1, "num",
      BREAKPOINT, "src/gap.c:BREAKPOINT" },

    { "MASTER_POINTER_NUMBER", 1, "ob",
      FuncMASTER_POINTER_NUMBER, "src/gap.c:MASTER_POINTER_NUMBER" },

    { "FUNC_BODY_SIZE", 1, "f",
      FuncFUNC_BODY_SIZE, "src/gap.c:FUNC_BODY_SIZE" },

    { "PRINT_CURRENT_STATEMENT", 1, "context",
      FuncPrintExecutingStatement, "src/gap.c:PRINT_CURRENT_STATEMENT" },

  
    { 0 }

};


/****************************************************************************
**

*F  InitKernel( <module> )  . . . . . . . . initialise kernel data structures
*/
static Int InitKernel (
    StructInitInfo *    module )
{
    /* init the completion function                                        */
    /* InitGlobalBag( &ThrownObject,      "src/gap.c:ThrownObject"      ); */

    /* list of exit functions                                              */
    InitGlobalBag( &AtExitFunctions, "src/gap.c:AtExitFunctions" );
    InitGlobalBag( &WindowCmdString, "src/gap.c:WindowCmdString" );

    /* init filters and functions                                          */
    InitHdlrFuncsFromTable( GVarFuncs );



    /* establish Fopy of ViewObj                                           */
    ImportFuncFromLibrary(  "ViewObj", 0L );
    ImportFuncFromLibrary(  "Error", &Error );
    ImportFuncFromLibrary(  "ErrorInner", &ErrorInner );

    DeclareGVar(&GVarTHREAD_INIT, "THREAD_INIT");
    DeclareGVar(&GVarTHREAD_EXIT, "THREAD_EXIT");


#if HAVE_SELECT
    InitCopyGVar("OnCharReadHookActive",&OnCharReadHookActive);
    InitCopyGVar("OnCharReadHookInFds",&OnCharReadHookInFds);
    InitCopyGVar("OnCharReadHookInFuncs",&OnCharReadHookInFuncs);
    InitCopyGVar("OnCharReadHookOutFds",&OnCharReadHookOutFds);
    InitCopyGVar("OnCharReadHookOutFuncs",&OnCharReadHookOutFuncs);
    InitCopyGVar("OnCharReadHookExcFds",&OnCharReadHookExcFds);
    InitCopyGVar("OnCharReadHookExcFuncs",&OnCharReadHookExcFuncs);
#endif

    /* If a package or .gaprc or file read from the command line
       sets this to a function, then we want to know                       */
    InitCopyGVar(  "AlternativeMainLoop", &AlternativeMainLoop );

    InitGlobalBag(&ErrorHandler, "gap.c: ErrorHandler");

    /* return success                                                      */
    return 0;
}


/****************************************************************************
**
*F  PostRestore( <module> ) . . . . . . . . . . . . . after restore workspace
*/
static Int PostRestore (
    StructInitInfo *    module )
{
      UInt var;

    /* library name and other stuff                                        */
    var = GVarName( "DEBUG_LOADING" );
    MakeReadWriteGVar(var);
    AssGVar( var, (SyDebugLoading ? True : False) );
    MakeReadOnlyGVar(var);

    /* construct the `ViewObj' variable                                    */
    ViewObjGVar = GVarName( "ViewObj" ); 
    CustomViewGVar = GVarName( "CustomView" ); 

    /* construct the last and time variables                               */
    Last              = GVarName( "last"  );
    Last2             = GVarName( "last2" );
    Last3             = GVarName( "last3" );
    Time              = GVarName( "time"  );
    SaveOnExitFileGVar= GVarName( "SaveOnExitFile" );
    QUITTINGGVar      = GVarName( "QUITTING" );
    
    /* return success                                                      */
    return 0;
}


/****************************************************************************
**
*F  InitLibrary( <module> ) . . . . . . .  initialise library data structures
*/
static Int InitLibrary (
    StructInitInfo *    module )
{



    /* init filters and functions                                          */
    InitGVarFuncsFromTable( GVarFuncs );

    /* create windows command buffer                                       */
    WindowCmdString = NEW_STRING( 1000 );
    /* return success                                                      */
    return PostRestore( module );
}


/****************************************************************************
**
*F  InitInfoGap() . . . . . . . . . . . . . . . . . . table of init functions
*/
static StructInitInfo module = {
    MODULE_BUILTIN,                     /* type                           */
    "gap",                              /* name                           */
    0,                                  /* revision entry of c file       */
    0,                                  /* revision entry of h file       */
    0,                                  /* version                        */
    0,                                  /* crc                            */
    InitKernel,                         /* initKernel                     */
    InitLibrary,                        /* initLibrary                    */
    0,                                  /* checkInit                      */
    0,                                  /* preSave                        */
    0,                                  /* postSave                       */
    PostRestore                         /* postRestore                    */
};

StructInitInfo * InitInfoGap ( void )
{
    return &module;
}


/****************************************************************************
**

*V  InitFuncsBuiltinModules . . . . .  list of builtin modules init functions
*/
static InitInfoFunc InitFuncsBuiltinModules[] = {

    /* global variables                                                    */
    InitInfoGVars,

    /* objects                                                             */
    InitInfoObjects,

    /* scanner, reader, interpreter, coder, caller, compiler               */
    InitInfoScanner,
    InitInfoRead,
    InitInfoCalls,
    InitInfoExprs,
    InitInfoStats,
    InitInfoCode,
    InitInfoVars,       /* must come after InitExpr and InitStats */
    InitInfoFuncs,
    InitInfoOpers,
    InitInfoIntrprtr,
    InitInfoCompiler,

    /* arithmetic operations                                               */
    InitInfoAriths,
    InitInfoInt,
    InitInfoIntFuncs,
    InitInfoRat,
    InitInfoCyc,
    InitInfoFinfield,
    InitInfoPermutat,
    InitInfoBool,
    InitInfoMacfloat,

    /* record packages                                                     */
    InitInfoRecords,
    InitInfoPRecord,

    /* list packages                                                       */
    InitInfoLists,
    InitInfoListOper,
    InitInfoListFunc,
    InitInfoPlist,
    InitInfoSet,
    InitInfoVector,
    InitInfoVecFFE,
    InitInfoBlist,
    InitInfoRange,
    InitInfoString,
    InitInfoGF2Vec,
    InitInfoVec8bit,

    /* free and presented groups                                           */
    InitInfoFreeGroupElements,
    InitInfoCosetTable,
    InitInfoTietze,
    InitInfoPcElements,
    InitInfoSingleCollector,
    InitInfoCombiCollector,
    InitInfoPcc,
    InitInfoDeepThought,
    InitInfoDTEvaluation,

    /* algebras                                                            */
    InitInfoSCTable,

    /* save and load workspace, weak pointers                              */
    InitInfoWeakPtr,
    InitInfoSaveLoad,

    /* input and output                                                    */
    InitInfoStreams,
    InitInfoSysFiles,
    InitInfoIOStream,

    /* main module                                                         */
    InitInfoGap,

    /* threads                                                             */
    InitInfoThreadAPI,
    InitInfoAObjects,
    InitInfoObjSets,

#ifdef GAPMPI
    /* ParGAP/MPI module                                                   */
    InitInfoGapmpi,
#endif

#ifdef WITH_ZMQ
    /* ZeroMQ module */
    InitInfoZmq,
#endif

    0
};


/****************************************************************************
**
*F  Modules . . . . . . . . . . . . . . . . . . . . . . . . . list of modules
*/
#ifndef MAX_MODULES
#define MAX_MODULES     1000
#endif


#ifndef MAX_MODULE_FILENAMES
#define MAX_MODULE_FILENAMES (MAX_MODULES*50)
#endif

Char LoadedModuleFilenames[MAX_MODULE_FILENAMES];
Char *NextLoadedModuleFilename = LoadedModuleFilenames;


StructInitInfo * Modules [ MAX_MODULES ];
UInt NrModules;
UInt NrBuiltinModules;


/****************************************************************************
**
*F  RecordLoadedModule( <module> )  . . . . . . . . store module in <Modules>
*/

void RecordLoadedModule (
    StructInitInfo *        info,
    Char *filename )
{
  UInt len;
    if ( NrModules == MAX_MODULES ) {
        Pr( "panic: no room to record module\n", 0L, 0L );
    }
    len = SyStrlen(filename);
    if (NextLoadedModuleFilename + len + 1
        > LoadedModuleFilenames+MAX_MODULE_FILENAMES) {
      Pr( "panic: no room for module filename\n", 0L, 0L );
    }
    *NextLoadedModuleFilename = '\0';
    SyStrncat(NextLoadedModuleFilename,filename, len);
    info->filename = NextLoadedModuleFilename;
    NextLoadedModuleFilename += len +1;
    Modules[NrModules++] = info;
}


/****************************************************************************
**
*F  InitializeGap() . . . . . . . . . . . . . . . . . . . . . . intialize GAP
**
**  Each module  (builtin  or compiled) exports  a sturctures  which contains
**  information about the name, version, crc, init function, save and restore
**  functions.
**
**  The init process is split into three different functions:
**
**  `InitKernel':   This function setups the   internal  data structures  and
**  tables,   registers the global bags  and   functions handlers, copies and
**  fopies.  It is not allowed to create objects, gvar or rnam numbers.  This
**  function is used for both starting and restoring.
**
**  `InitLibrary': This function creates objects,  gvar and rnam number,  and
**  does  assignments of auxillary C   variables (for example, pointers  from
**  objects, length of hash lists).  This function is only used for starting.
**
**  `PostRestore': Everything in  `InitLibrary' execpt  creating objects.  In
**  general    `InitLibrary'  will  create    all objects    and  then  calls
**  `PostRestore'.  This function is only used when restoring.
*/
#ifndef BOEHM_GC
extern TNumMarkFuncBags TabMarkFuncBags [ 256 ];
#endif

static Obj POST_RESTORE;

void InitializeGap (
    int *               pargc,
    char *              argv [] )
{
  /*    UInt                type; */
    UInt                i;
    Int                 ret;


    /* initialize the basic system and gasman                              */
#ifdef GAPMPI
    /* ParGAP/MPI needs to call MPI_Init() first to remove command line args */
    InitGapmpi( pargc, &argv );
#endif

    InitSystem( *pargc, argv );

    /* Initialise memory  -- have to do this here to make sure we are at top of C stack */
    InitBags( SyAllocBags, SyStorMin,
              0, (Bag*)(((UInt)pargc/SyStackAlign)*SyStackAlign), SyStackAlign,
              SyCacheSize, 0, SyAbortBags );
              InitMsgsFuncBags( SyMsgsBags ); 


    /* get info structures for the build in modules                        */
    NrModules = 0;
    for ( i = 0;  InitFuncsBuiltinModules[i];  i++ ) {
        if ( NrModules == MAX_MODULES ) {
            FPUTS_TO_STDERR( "panic: too many builtin modules\n" );
            SyExit(1);
        }
        Modules[NrModules++] = InitFuncsBuiltinModules[i]();
#       ifdef DEBUG_LOADING
            FPUTS_TO_STDERR( "#I  InitInfo(builtin " );
            FPUTS_TO_STDERR( Modules[NrModules-1]->name );
            FPUTS_TO_STDERR( ")\n" );
#       endif
    }
    NrBuiltinModules = NrModules;

    /* call kernel initialisation                                          */
    for ( i = 0;  i < NrBuiltinModules;  i++ ) {
        if ( Modules[i]->initKernel ) {
#           ifdef DEBUG_LOADING
                FPUTS_TO_STDERR( "#I  InitKernel(builtin " );
                FPUTS_TO_STDERR( Modules[i]->name );
                FPUTS_TO_STDERR( ")\n" );
#           endif
            ret =Modules[i]->initKernel( Modules[i] );
            if ( ret ) {
                FPUTS_TO_STDERR( "#I  InitKernel(builtin " );
                FPUTS_TO_STDERR( Modules[i]->name );
                FPUTS_TO_STDERR( ") returned non-zero value\n" );
            }
        }
    }

    InitMainThread();
    InitTLS();

    InitGlobalBag(&POST_RESTORE, "gap.c: POST_RESTORE");
    InitFopyGVar( "POST_RESTORE", &POST_RESTORE);

    /* you should set 'COUNT_BAGS' as well                                 */
#   ifdef DEBUG_LOADING
        if ( SyRestoring ) {
            Pr( "#W  after setup\n", 0L, 0L );
            Pr( "#W  %36s ", (Int)"type",  0L          );
            Pr( "%8s %8s ",  (Int)"alive", (Int)"kbyte" );
            Pr( "%8s %8s\n",  (Int)"total", (Int)"kbyte" );
            for ( i = 0;  i < 256;  i++ ) {
                if ( InfoBags[i].name != 0 && InfoBags[i].nrAll != 0 ) {
                    char    buf[41];

                    buf[0] = '\0';
                    SyStrncat( buf, InfoBags[i].name, 40 );
                    Pr("#W  %36s ",    (Int)buf, 0L );
                    Pr("%8d %8d ", (Int)InfoBags[i].nrLive,
                       (Int)(InfoBags[i].sizeLive/1024));
                    Pr("%8d %8d\n",(Int)InfoBags[i].nrAll,
                       (Int)(InfoBags[i].sizeAll/1024));
                }
            }
        }
#   endif

#ifndef BOEHM_GC
    /* and now for a special hack                                          */
    for ( i = LAST_CONSTANT_TNUM+1; i <= LAST_REAL_TNUM; i++ ) {
      if (TabMarkFuncBags[i + COPYING] == MarkAllSubBagsDefault)
        TabMarkFuncBags[ i+COPYING ] = TabMarkFuncBags[ i ];
    }
#endif

    /* if we are restoring, load the workspace and call the post restore   */
    if ( SyRestoring ) {
       LoadWorkspace(SyRestoring);
        for ( i = 0;  i < NrModules;  i++ ) {
            if ( Modules[i]->postRestore ) {
#               ifdef DEBUG_LOADING
                    FPUTS_TO_STDERR( "#I  PostRestore(builtin " );
                    FPUTS_TO_STDERR( Modules[i]->name );
                    FPUTS_TO_STDERR( ")\n" );
#               endif
                ret = Modules[i]->postRestore( Modules[i] );
                if ( ret ) {
                    FPUTS_TO_STDERR( "#I  PostRestore(builtin " );
                    FPUTS_TO_STDERR( Modules[i]->name );
                    FPUTS_TO_STDERR( ") returned non-zero value\n" );
                }
            }
        }
        SyRestoring = NULL;


        /* Call POST_RESTORE which is a GAP function that now takes control, 
           calls the post restore functions and then runs a GAP session */
        if (POST_RESTORE != (Obj) 0 &&
            IS_FUNC(POST_RESTORE))
          if (!READ_ERROR())
            CALL_0ARGS(POST_RESTORE);
    }


    /* otherwise call library initialisation                               */
    else {
        WarnInitGlobalBag = 1;
#       ifdef DEBUG_HANDLER_REGISTRATION
            CheckAllHandlers();
#       endif

        SyInitializing = 1;    
        for ( i = 0;  i < NrBuiltinModules;  i++ ) {
            if ( Modules[i]->initLibrary ) {
#               ifdef DEBUG_LOADING
                    FPUTS_TO_STDERR( "#I  InitLibrary(builtin " );
                    FPUTS_TO_STDERR( Modules[i]->name );
                    FPUTS_TO_STDERR( ")\n" );
#               endif
                ret = Modules[i]->initLibrary( Modules[i] );
                if ( ret ) {
                    FPUTS_TO_STDERR( "#I  InitLibrary(builtin " );
                    FPUTS_TO_STDERR( Modules[i]->name );
                    FPUTS_TO_STDERR( ") returned non-zero value\n" );
                }
            }
        }
        WarnInitGlobalBag = 0;
    }

    /* check initialisation                                                */
    for ( i = 0;  i < NrModules;  i++ ) {
        if ( Modules[i]->checkInit ) {
#           ifdef DEBUG_LOADING
                FPUTS_TO_STDERR( "#I  CheckInit(builtin " );
                FPUTS_TO_STDERR( Modules[i]->name );
                FPUTS_TO_STDERR( ")\n" );
#           endif
            ret = Modules[i]->checkInit( Modules[i] );
            if ( ret ) {
                FPUTS_TO_STDERR( "#I  CheckInit(builtin " );
                FPUTS_TO_STDERR( Modules[i]->name );
                FPUTS_TO_STDERR( ") returned non-zero value\n" );
            }
        }
    }

    /* read the init files      
       this now actually runs the GAP session, we only get 
       past here when we're about to exit. 
                                           */
    if ( SySystemInitFile[0] ) {
      if (!READ_ERROR()) {
        if ( READ_GAP_ROOT(SySystemInitFile) == 0 ) {
          /*             if ( ! SyQuiet ) { */
                Pr( "gap: hmm, I cannot find '%s' maybe",
                    (Int)SySystemInitFile, 0L );
                Pr( " use option '-l <gaproot>'?\n If you ran the GAP"
                    " binary directly, try running the 'gap.sh' or 'gap.bat'"
                    " script instead.", 0L, 0L );
            }
      }
      else
        {
          Pr("Caught error at top-most level, probably quit from library loading",0L,0L);
          SyExit(1);
        }
        /*         } */
    }

}

/****************************************************************************
**

*E  gap.c . . . . . . . . . . . . . . . . . . . . . . . . . . . . . ends here
*/



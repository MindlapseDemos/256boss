/****************************************************************************

		THIS SOFTWARE IS NOT COPYRIGHTED

   HP offers the following for use in the public domain.  HP makes no
   warranty with regard to the software or it's performance and the
   user accepts the software "AS IS" with all faults.

   HP DISCLAIMS ANY WARRANTIES, EXPRESS OR IMPLIED, WITH REGARD
   TO THIS SOFTWARE INCLUDING BUT NOT LIMITED TO THE WARRANTIES
   OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.

****************************************************************************/

/****************************************************************************
 *  Header: remcom.c,v 1.34 91/03/09 12:29:49 glenne Exp $
 *
 *  Module name: remcom.c $
 *  Revision: 1.34 $
 *  Date: 91/03/09 12:29:49 $
 *  Contributor:     Lake Stevens Instrument Division$
 *
 *  Description:     low level support for gdb debugger. $
 *
 *  Considerations:  only works on target hardware $
 *
 *  Written by:      Glenn Engel $
 *  ModuleState:     Experimental $
 *
 *  NOTES:           See Below $
 *
 *  Modified for 386 by Jim Kingdon, Cygnus Support.
 *
 *  To enable debugger support, two things need to happen.  One, a
 *  call to set_debug_traps() is necessary in order to allow any breakpoints
 *  or error conditions to be properly intercepted and reported to gdb.
 *  Two, a breakpoint needs to be generated to begin communication.  This
 *  is most easily accomplished by a call to breakpoint().  Breakpoint()
 *  simulates a breakpoint by executing a trap #1.
 *
 *  The external function exceptionHandler() is
 *  used to attach a specific handler to a specific 386 vector number.
 *  It should use the same privilege level it runs at.  It should
 *  install it as an interrupt gate so that interrupts are masked
 *  while the handler runs.
 *
 *  Because gdb will sometimes write to the stack area to execute function
 *  calls, this program cannot rely on using the supervisor stack so it
 *  uses it's own stack area reserved in the int array remcomStack.
 *
 *************
 *
 *    The following gdb commands are supported:
 *
 * command          function                               Return value
 *
 *    g             return the value of the CPU registers  hex data or ENN
 *    G             set the value of the CPU registers     OK or ENN
 *
 *    mAA..AA,LLLL  Read LLLL bytes at address AA..AA      hex data or ENN
 *    MAA..AA,LLLL: Write LLLL bytes at address AA.AA      OK or ENN
 *
 *    c             Resume at current address              SNN   ( signal NN)
 *    cAA..AA       Continue at address AA..AA             SNN
 *
 *    s             Step one instruction                   SNN
 *    sAA..AA       Step one instruction from AA..AA       SNN
 *
 *    k             kill
 *
 *    ?             What was the last sigval ?             SNN   (signal NN)
 *
 * All commands and responses are sent with a packet which includes a
 * checksum.  A packet consists of
 *
 * $<packet info>#<checksum>.
 *
 * where
 * <packet info> :: <characters representing the command or response>
 * <checksum>    :: < two hex digits computed as modulo 256 sum of <packetinfo>>
 *
 * When a packet is received, it is first acknowledged with either '+' or '-'.
 * '+' indicates a successful transfer.  '-' indicates a failed transfer.
 *
 * Example:
 *
 * Host:                  Reply:
 * $m0,10#2a               +$00010203040506070809101112131415#42
 *
 ****************************************************************************/
#include "config.h"
#ifdef ENABLE_GDB_STUB

#include <stdio.h>
#include <string.h>

/************************************************************************
 *
 * external low-level support routines
 */

extern void putDebugChar();	/* write a single character      */
extern int getDebugChar();	/* read and return a single char */
extern void exceptionHandler();	/* assign an exception handler   */

/************************************************************************/
/* BUFMAX defines the maximum number of characters in inbound/outbound buffers*/
/* at least NUMREGBYTES*2 are needed for register packets */
#define BUFMAX 400

static char initialized;  /* boolean flag. != 0 means we've been initialized */

int     remote_debug;
/*  debug >  0 prints ill-formed commands in valid packets & checksum errors */

static const char hexchars[]="0123456789abcdef";

/* Number of registers.  */
#define NUMREGS	16

/* Number of bytes of registers.  */
#define NUMREGBYTES (NUMREGS * 4)

enum regnames {EAX, ECX, EDX, EBX, ESP, EBP, ESI, EDI,
	       PC /* also known as eip */,
	       PS /* also known as eflags */,
	       CS, SS, DS, ES, FS, GS};

/*
 * these should not be static cuz they can be used outside this module
 */
int registers[NUMREGS];

#define STACKSIZE 10000
int remcomStack[STACKSIZE/sizeof(int)];
static int* stackPtr = &remcomStack[STACKSIZE/sizeof(int) - 1];

/***************************  ASSEMBLY CODE MACROS *************************/
/* 									   */

extern void
return_to_prog ();

/* Restore the program's registers (including the stack pointer, which
   means we get the right stack and don't have to worry about popping our
   return address and any stack frames and so on) and return.  */
asm(".text");
asm(".globl return_to_prog");
asm("return_to_prog:");
asm("        movw registers+44, %ss");
asm("        movl registers+16, %esp");
asm("        movl registers+4, %ecx");
asm("        movl registers+8, %edx");
asm("        movl registers+12, %ebx");
asm("        movl registers+20, %ebp");
asm("        movl registers+24, %esi");
asm("        movl registers+28, %edi");
asm("        movw registers+48, %ds");
asm("        movw registers+52, %es");
asm("        movw registers+56, %fs");
asm("        movw registers+60, %gs");
asm("        movl registers+36, %eax");
asm("        pushl %eax");  /* saved eflags */
asm("        movl registers+40, %eax");
asm("        pushl %eax");  /* saved cs */
asm("        movl registers+32, %eax");
asm("        pushl %eax");  /* saved eip */
asm("        movl registers, %eax");
/* use iret to restore pc and flags together so
   that trace flag works right.  */
asm("        iret");

#define BREAKPOINT() asm("   int $3");

/* Put the error code here just in case the user cares.  */
int gdb_i386errcode;
/* Likewise, the vector number here (since GDB only gets the signal
   number through the usual means, and that's not very specific).  */
int gdb_i386vector = -1;

/* GDB stores segment registers in 32-bit words (that's just the way
   m-i386v.h is written).  So zero the appropriate areas in registers.  */
#define SAVE_REGISTERS1() \
  asm ("movl %eax, registers");                                   	  \
  asm ("movl %ecx, registers+4");			  		     \
  asm ("movl %edx, registers+8");			  		     \
  asm ("movl %ebx, registers+12");			  		     \
  asm ("movl %ebp, registers+20");			  		     \
  asm ("movl %esi, registers+24");			  		     \
  asm ("movl %edi, registers+28");			  		     \
  asm ("movw $0, %ax");							     \
  asm ("movw %ds, registers+48");			  		     \
  asm ("movw %ax, registers+50");					     \
  asm ("movw %es, registers+52");			  		     \
  asm ("movw %ax, registers+54");					     \
  asm ("movw %fs, registers+56");			  		     \
  asm ("movw %ax, registers+58");					     \
  asm ("movw %gs, registers+60");			  		     \
  asm ("movw %ax, registers+62")
#define SAVE_ERRCODE() \
  asm ("popl %ebx");                                  \
  asm ("movl %ebx, gdb_i386errcode")
#define SAVE_REGISTERS2() \
  asm ("popl %ebx"); /* old eip */			  		     \
  asm ("movl %ebx, registers+32");			  		     \
  asm ("popl %ebx");	 /* old cs */			  		     \
  asm ("movl %ebx, registers+40");			  		     \
  asm ("movw %ax, registers+42");                                           \
  asm ("popl %ebx");	 /* old eflags */		  		     \
  asm ("movl %ebx, registers+36");			 		     \
  /* Now that we've done the pops, we can save the stack pointer.");  */   \
  asm ("movw %ss, registers+44");					     \
  asm ("movw %ax, registers+46");     	       	       	       	       	     \
  asm ("movl %esp, registers+16")

/* See if mem_fault_routine is set, if so just IRET to that address.  */
#define CHECK_FAULT() \
  asm ("cmpl $0, mem_fault_routine");					   \
  asm ("jne mem_fault")

asm (".text");
asm ("mem_fault:");
/* OK to clobber temp registers; we're just going to end up in set_mem_err.  */
/* Pop error code from the stack and save it.  */
asm ("     popl %eax");
asm ("     movl %eax, gdb_i386errcode");

asm ("     popl %eax"); /* eip */
/* We don't want to return there, we want to return to the function
   pointed to by mem_fault_routine instead.  */
asm ("     movl mem_fault_routine, %eax");
asm ("     popl %ecx"); /* cs (low 16 bits; junk in hi 16 bits).  */
asm ("     popl %edx"); /* eflags */

/* Remove this stack frame; when we do the iret, we will be going to
   the start of a function, so we want the stack to look just like it
   would after a "call" instruction.  */
asm ("     leave");

/* Push the stuff that iret wants.  */
asm ("     pushl %edx"); /* eflags */
asm ("     pushl %ecx"); /* cs */
asm ("     pushl %eax"); /* eip */

/* Zero mem_fault_routine.  */
asm ("     movl $0, %eax");
asm ("     movl %eax, mem_fault_routine");

asm ("iret");

#define CALL_HOOK() asm("call remcomHandler")

/* This function is called when a i386 exception occurs.  It saves
 * all the cpu regs in the registers array, munges the stack a bit,
 * and invokes an exception handler (remcomhandler).
 *
 * stack on entry:                       stack on exit:
 *   old eflags                          vector number
 *   old cs (zero-filled to 32 bits)
 *   old eip
 *
 */
extern void _catchException3();
asm(".text");
asm(".globl _catchException3");
asm("_catchException3:");
SAVE_REGISTERS1();
SAVE_REGISTERS2();
asm ("pushl $3");
CALL_HOOK();

/* Same thing for exception 1.  */
extern void _catchException1();
asm(".text");
asm(".globl _catchException1");
asm("_catchException1:");
SAVE_REGISTERS1();
SAVE_REGISTERS2();
asm ("pushl $1");
CALL_HOOK();

/* Same thing for exception 0.  */
extern void _catchException0();
asm(".text");
asm(".globl _catchException0");
asm("_catchException0:");
SAVE_REGISTERS1();
SAVE_REGISTERS2();
asm ("pushl $0");
CALL_HOOK();

/* Same thing for exception 4.  */
extern void _catchException4();
asm(".text");
asm(".globl _catchException4");
asm("_catchException4:");
SAVE_REGISTERS1();
SAVE_REGISTERS2();
asm ("pushl $4");
CALL_HOOK();

/* Same thing for exception 5.  */
extern void _catchException5();
asm(".text");
asm(".globl _catchException5");
asm("_catchException5:");
SAVE_REGISTERS1();
SAVE_REGISTERS2();
asm ("pushl $5");
CALL_HOOK();

/* Same thing for exception 6.  */
extern void _catchException6();
asm(".text");
asm(".globl _catchException6");
asm("_catchException6:");
SAVE_REGISTERS1();
SAVE_REGISTERS2();
asm ("pushl $6");
CALL_HOOK();

/* Same thing for exception 7.  */
extern void _catchException7();
asm(".text");
asm(".globl _catchException7");
asm("_catchException7:");
SAVE_REGISTERS1();
SAVE_REGISTERS2();
asm ("pushl $7");
CALL_HOOK();

/* Same thing for exception 8.  */
extern void _catchException8();
asm(".text");
asm(".globl _catchException8");
asm("_catchException8:");
SAVE_REGISTERS1();
SAVE_ERRCODE();
SAVE_REGISTERS2();
asm ("pushl $8");
CALL_HOOK();

/* Same thing for exception 9.  */
extern void _catchException9();
asm(".text");
asm(".globl _catchException9");
asm("_catchException9:");
SAVE_REGISTERS1();
SAVE_REGISTERS2();
asm ("pushl $9");
CALL_HOOK();

/* Same thing for exception 10.  */
extern void _catchException10();
asm(".text");
asm(".globl _catchException10");
asm("_catchException10:");
SAVE_REGISTERS1();
SAVE_ERRCODE();
SAVE_REGISTERS2();
asm ("pushl $10");
CALL_HOOK();

/* Same thing for exception 12.  */
extern void _catchException12();
asm(".text");
asm(".globl _catchException12");
asm("_catchException12:");
SAVE_REGISTERS1();
SAVE_ERRCODE();
SAVE_REGISTERS2();
asm ("pushl $12");
CALL_HOOK();

/* Same thing for exception 16.  */
extern void _catchException16();
asm(".text");
asm(".globl _catchException16");
asm("_catchException16:");
SAVE_REGISTERS1();
SAVE_REGISTERS2();
asm ("pushl $16");
CALL_HOOK();

/* For 13, 11, and 14 we have to deal with the CHECK_FAULT stuff.  */

/* Same thing for exception 13.  */
extern void _catchException13 ();
asm (".text");
asm (".globl _catchException13");
asm ("_catchException13:");
CHECK_FAULT();
SAVE_REGISTERS1();
SAVE_ERRCODE();
SAVE_REGISTERS2();
asm ("pushl $13");
CALL_HOOK();

/* Same thing for exception 11.  */
extern void _catchException11 ();
asm (".text");
asm (".globl _catchException11");
asm ("_catchException11:");
CHECK_FAULT();
SAVE_REGISTERS1();
SAVE_ERRCODE();
SAVE_REGISTERS2();
asm ("pushl $11");
CALL_HOOK();

/* Same thing for exception 14.  */
extern void _catchException14 ();
asm (".text");
asm (".globl _catchException14");
asm ("_catchException14:");
CHECK_FAULT();
SAVE_REGISTERS1();
SAVE_ERRCODE();
SAVE_REGISTERS2();
asm ("pushl $14");
CALL_HOOK();

/*
 * remcomHandler is a front end for handle_exception.  It moves the
 * stack pointer into an area reserved for debugger use.
 */
asm("remcomHandler:");
asm("           popl %eax");        /* pop off return address     */
asm("           popl %eax");      /* get the exception number   */
asm("		movl stackPtr, %esp"); /* move to remcom stack area  */
asm("		pushl %eax");	/* push exception onto stack  */
asm("		call  handle_exception");    /* this never returns */

void
_returnFromException ()
{
  return_to_prog ();
}

int
hex (ch)
     char ch;
{
  if ((ch >= 'a') && (ch <= 'f'))
    return (ch - 'a' + 10);
  if ((ch >= '0') && (ch <= '9'))
    return (ch - '0');
  if ((ch >= 'A') && (ch <= 'F'))
    return (ch - 'A' + 10);
  return (-1);
}

static char remcomInBuffer[BUFMAX];
static char remcomOutBuffer[BUFMAX];

/* scan for the sequence $<data>#<checksum>     */

unsigned char *
getpacket (void)
{
  unsigned char *buffer = (unsigned char*)&remcomInBuffer[0];
  unsigned char checksum;
  unsigned char xmitcsum;
  int count;
  char ch;

  while (1)
    {
      /* wait around for the start character, ignore all other characters */
      while ((ch = getDebugChar ()) != '$')
	;

    retry:
      checksum = 0;
      xmitcsum = -1;
      count = 0;

      /* now, read until a # or end of buffer is found */
      while (count < BUFMAX - 1)
	{
	  ch = getDebugChar ();
	  if (ch == '$')
	    goto retry;
	  if (ch == '#')
	    break;
	  checksum = checksum + ch;
	  buffer[count] = ch;
	  count = count + 1;
	}
      buffer[count] = 0;

      if (ch == '#')
	{
	  ch = getDebugChar ();
	  xmitcsum = hex (ch) << 4;
	  ch = getDebugChar ();
	  xmitcsum += hex (ch);

	  if (checksum != xmitcsum)
	    {
	      if (remote_debug)
		{
		  fprintf (stderr,
			   "bad checksum.  My count = 0x%x, sent=0x%x. buf=%s\n",
			   checksum, xmitcsum, buffer);
		}
	      putDebugChar ('-');	/* failed checksum */
	    }
	  else
	    {
	      putDebugChar ('+');	/* successful transfer */

	      /* if a sequence char is present, reply the sequence ID */
	      if (buffer[2] == ':')
		{
		  putDebugChar (buffer[0]);
		  putDebugChar (buffer[1]);

		  return &buffer[3];
		}

	      return &buffer[0];
	    }
	}
    }
}

/* send the packet in buffer.  */

void
putpacket (unsigned char *buffer)
{
  unsigned char checksum;
  int count;
  char ch;

  /*  $<packet info>#<checksum>.  */
  do
    {
      putDebugChar ('$');
      checksum = 0;
      count = 0;

      while ((ch = buffer[count]))
	{
	  putDebugChar (ch);
	  checksum += ch;
	  count += 1;
	}

      putDebugChar ('#');
      putDebugChar (hexchars[checksum >> 4]);
      putDebugChar (hexchars[checksum % 16]);

    }
  while (getDebugChar () != '+');
}

void
debug_error (format, parm)
     char *format;
     char *parm;
{
  if (remote_debug)
    fprintf (stderr, format, parm);
}

/* Address of a routine to RTE to if we get a memory fault.  */
static void (*volatile mem_fault_routine) () = NULL;

/* Indicate to caller of mem2hex or hex2mem that there has been an
   error.  */
static volatile int mem_err = 0;

void
set_mem_err (void)
{
  mem_err = 1;
}

/* These are separate functions so that they are so short and sweet
   that the compiler won't save any registers (if there is a fault
   to mem_fault, they won't get restored, so there better not be any
   saved).  */
int
get_char (char *addr)
{
  return *addr;
}

void
set_char (char *addr, int val)
{
  *addr = val;
}

/* convert the memory pointed to by mem into hex, placing result in buf */
/* return a pointer to the last char put in buf (null) */
/* If MAY_FAULT is non-zero, then we should set mem_err in response to
   a fault; if zero treat a fault like any other fault in the stub.  */
char *
mem2hex (mem, buf, count, may_fault)
     char *mem;
     char *buf;
     int count;
     int may_fault;
{
  int i;
  unsigned char ch;

  if (may_fault)
    mem_fault_routine = set_mem_err;
  for (i = 0; i < count; i++)
    {
      ch = get_char (mem++);
      if (may_fault && mem_err)
	return (buf);
      *buf++ = hexchars[ch >> 4];
      *buf++ = hexchars[ch % 16];
    }
  *buf = 0;
  if (may_fault)
    mem_fault_routine = NULL;
  return (buf);
}

/* convert the hex array pointed to by buf into binary to be placed in mem */
/* return a pointer to the character AFTER the last byte written */
char *
hex2mem (buf, mem, count, may_fault)
     char *buf;
     char *mem;
     int count;
     int may_fault;
{
  int i;
  unsigned char ch;

  if (may_fault)
    mem_fault_routine = set_mem_err;
  for (i = 0; i < count; i++)
    {
      ch = hex (*buf++) << 4;
      ch = ch + hex (*buf++);
      set_char (mem++, ch);
      if (may_fault && mem_err)
	return (mem);
    }
  if (may_fault)
    mem_fault_routine = NULL;
  return (mem);
}

/* this function takes the 386 exception vector and attempts to
   translate this number into a unix compatible signal value */
int
computeSignal (int exceptionVector)
{
  int sigval;
  switch (exceptionVector)
    {
    case 0:
      sigval = 8;
      break;			/* divide by zero */
    case 1:
      sigval = 5;
      break;			/* debug exception */
    case 3:
      sigval = 5;
      break;			/* breakpoint */
    case 4:
      sigval = 16;
      break;			/* into instruction (overflow) */
    case 5:
      sigval = 16;
      break;			/* bound instruction */
    case 6:
      sigval = 4;
      break;			/* Invalid opcode */
    case 7:
      sigval = 8;
      break;			/* coprocessor not available */
    case 8:
      sigval = 7;
      break;			/* double fault */
    case 9:
      sigval = 11;
      break;			/* coprocessor segment overrun */
    case 10:
      sigval = 11;
      break;			/* Invalid TSS */
    case 11:
      sigval = 11;
      break;			/* Segment not present */
    case 12:
      sigval = 11;
      break;			/* stack exception */
    case 13:
      sigval = 11;
      break;			/* general protection */
    case 14:
      sigval = 11;
      break;			/* page fault */
    case 16:
      sigval = 7;
      break;			/* coprocessor error */
    default:
      sigval = 7;		/* "software generated" */
    }
  return (sigval);
}

/**********************************************/
/* WHILE WE FIND NICE HEX CHARS, BUILD AN INT */
/* RETURN NUMBER OF CHARS PROCESSED           */
/**********************************************/
int
hexToInt (char **ptr, int *intValue)
{
  int numChars = 0;
  int hexValue;

  *intValue = 0;

  while (**ptr)
    {
      hexValue = hex (**ptr);
      if (hexValue >= 0)
	{
	  *intValue = (*intValue << 4) | hexValue;
	  numChars++;
	}
      else
	break;

      (*ptr)++;
    }

  return (numChars);
}

/*
 * This function does all command procesing for interfacing to gdb.
 */
void
handle_exception (int exceptionVector)
{
  int sigval, stepping;
  int addr, length;
  char *ptr;
  /*int newPC;*/

  gdb_i386vector = exceptionVector;

  if (remote_debug)
    {
      printf ("vector=%d, sr=0x%x, pc=0x%x\n",
	      exceptionVector, registers[PS], registers[PC]);
    }

  /* reply to host that an exception has occurred */
  sigval = computeSignal (exceptionVector);

  ptr = remcomOutBuffer;

  *ptr++ = 'T';			/* notify gdb with signo, PC, FP and SP */
  *ptr++ = hexchars[sigval >> 4];
  *ptr++ = hexchars[sigval & 0xf];

  *ptr++ = hexchars[ESP];
  *ptr++ = ':';
  ptr = mem2hex((char *)&registers[ESP], ptr, 4, 0);	/* SP */
  *ptr++ = ';';

  *ptr++ = hexchars[EBP]; 
  *ptr++ = ':';
  ptr = mem2hex((char *)&registers[EBP], ptr, 4, 0); 	/* FP */
  *ptr++ = ';';

  *ptr++ = hexchars[PC]; 
  *ptr++ = ':';
  ptr = mem2hex((char *)&registers[PC], ptr, 4, 0); 	/* PC */
  *ptr++ = ';';

  *ptr = '\0';

  putpacket ((unsigned char*)remcomOutBuffer);

  stepping = 0;

  while (1 == 1)
    {
      remcomOutBuffer[0] = 0;
      ptr = (char*)getpacket ();

      switch (*ptr++)
	{
	case '?':
	  remcomOutBuffer[0] = 'S';
	  remcomOutBuffer[1] = hexchars[sigval >> 4];
	  remcomOutBuffer[2] = hexchars[sigval % 16];
	  remcomOutBuffer[3] = 0;
	  break;
	case 'd':
	  remote_debug = !(remote_debug);	/* toggle debug flag */
	  break;
	case 'g':		/* return the value of the CPU registers */
	  mem2hex ((char *) registers, remcomOutBuffer, NUMREGBYTES, 0);
	  break;
	case 'G':		/* set the value of the CPU registers - return OK */
	  hex2mem (ptr, (char *) registers, NUMREGBYTES, 0);
	  strcpy (remcomOutBuffer, "OK");
	  break;
	case 'P':		/* set the value of a single CPU register - return OK */
	  {
	    int regno;

	    if (hexToInt (&ptr, &regno) && *ptr++ == '=')
	      if (regno >= 0 && regno < NUMREGS)
		{
		  hex2mem (ptr, (char *) &registers[regno], 4, 0);
		  strcpy (remcomOutBuffer, "OK");
		  break;
		}

	    strcpy (remcomOutBuffer, "E01");
	    break;
	  }

	  /* mAA..AA,LLLL  Read LLLL bytes at address AA..AA */
	case 'm':
	  /* TRY TO READ %x,%x.  IF SUCCEED, SET PTR = 0 */
	  if (hexToInt (&ptr, &addr))
	    if (*(ptr++) == ',')
	      if (hexToInt (&ptr, &length))
		{
		  ptr = 0;
		  mem_err = 0;
		  mem2hex ((char *) addr, remcomOutBuffer, length, 1);
		  if (mem_err)
		    {
		      strcpy (remcomOutBuffer, "E03");
		      debug_error ("memory fault");
		    }
		}

	  if (ptr)
	    {
	      strcpy (remcomOutBuffer, "E01");
	    }
	  break;

	  /* MAA..AA,LLLL: Write LLLL bytes at address AA.AA return OK */
	case 'M':
	  /* TRY TO READ '%x,%x:'.  IF SUCCEED, SET PTR = 0 */
	  if (hexToInt (&ptr, &addr))
	    if (*(ptr++) == ',')
	      if (hexToInt (&ptr, &length))
		if (*(ptr++) == ':')
		  {
		    mem_err = 0;
		    hex2mem (ptr, (char *) addr, length, 1);

		    if (mem_err)
		      {
			strcpy (remcomOutBuffer, "E03");
			debug_error ("memory fault");
		      }
		    else
		      {
			strcpy (remcomOutBuffer, "OK");
		      }

		    ptr = 0;
		  }
	  if (ptr)
	    {
	      strcpy (remcomOutBuffer, "E02");
	    }
	  break;

	  /* cAA..AA    Continue at address AA..AA(optional) */
	  /* sAA..AA   Step one instruction from AA..AA(optional) */
	case 's':
	  stepping = 1;
	case 'c':
	  /* try to read optional parameter, pc unchanged if no parm */
	  if (hexToInt (&ptr, &addr))
	    registers[PC] = addr;

	  /*newPC = registers[PC];*/

	  /* clear the trace bit */
	  registers[PS] &= 0xfffffeff;

	  /* set the trace bit if we're stepping */
	  if (stepping)
	    registers[PS] |= 0x100;

	  _returnFromException ();	/* this is a jump */
	  break;

	  /* kill the program */
	case 'k':		/* do nothing */
#if 0
	  /* Huh? This doesn't look like "nothing".
	     m68k-stub.c and sparc-stub.c don't have it.  */
	  BREAKPOINT ();
#endif
	  break;
	}			/* switch */

      /* reply to the request */
      putpacket ((unsigned char*)remcomOutBuffer);
    }
}

/* this function is used to set up exception handlers for tracing and
   breakpoints */
void
set_debug_traps (void)
{
  stackPtr = &remcomStack[STACKSIZE / sizeof (int) - 1];

  exceptionHandler (0, _catchException0);
  exceptionHandler (1, _catchException1);
  exceptionHandler (3, _catchException3);
  exceptionHandler (4, _catchException4);
  exceptionHandler (5, _catchException5);
  exceptionHandler (6, _catchException6);
  exceptionHandler (7, _catchException7);
  exceptionHandler (8, _catchException8);
  exceptionHandler (9, _catchException9);
  exceptionHandler (10, _catchException10);
  exceptionHandler (11, _catchException11);
  exceptionHandler (12, _catchException12);
  exceptionHandler (13, _catchException13);
  exceptionHandler (14, _catchException14);
  exceptionHandler (16, _catchException16);

  initialized = 1;
}

/* This function will generate a breakpoint exception.  It is used at the
   beginning of a program to sync up with a debugger and can be used
   otherwise as a quick means to stop program execution and "break" into
   the debugger.  */

void
breakpoint (void)
{
  if (initialized)
    BREAKPOINT ();
}

#else
int no_gdbstub_shutup_gcc_empty_warning;
#endif	/* ENABLE_GDB_STUB */

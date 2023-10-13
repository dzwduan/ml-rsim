/*
 * Copyright (c) 2002 The Board of Trustees of the University of Illinois and
 *                    William Marsh Rice University
 * Copyright (c) 2002 The University of Utah
 * Copyright (c) 2002 The University of Notre Dame du Lac
 *
 * All rights reserved.
 *
 * Based on RSIM 1.0, developed by:
 *   Professor Sarita Adve's RSIM research group
 *   University of Illinois at Urbana-Champaign and
     William Marsh Rice University
 *   http://www.cs.uiuc.edu/rsim and http://www.ece.rice.edu/~rsim/dist.html
 * ML-RSIM/URSIM extensions by:
 *   The Impulse Research Group, University of Utah
 *   http://www.cs.utah.edu/impulse
 *   Lambert Schaelicke, University of Utah and University of Notre Dame du Lac
 *   http://www.cse.nd.edu/~lambert
 *   Mike Parker, University of Utah
 *   http://www.cs.utah.edu/~map
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal with the Software without restriction, including without
 * limitation the rights to use, copy, modify, merge, publish, distribute,
 * sublicense, and/or sell copies of the Software, and to permit persons to
 * whom the Software is furnished to do so, subject to the following
 * conditions:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimers. 
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimers in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the names of Professor Sarita Adve's RSIM research group,
 *    the University of Illinois at Urbana-Champaign, William Marsh Rice
 *    University, nor the names of its contributors may be used to endorse
 *    or promote products derived from this Software without specific prior
 *    written permission. 
 * 4. Neither the names of the ML-RSIM project, the URSIM project, the
 *    Impulse research group, the University of Utah, the University of
 *    Notre Dame du Lac, nor the names of its contributors may be used to
 *    endorse or promote products derived from this software without specific
 *    prior written permission. 
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE CONTRIBUTORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS WITH THE SOFTWARE. 
 */

/*****************************************************************************/
/*                                                                           */
/* Realtime Clock (modeled after various existing chips, e.g. Analog Devices */
/*   Dallas DS1743, SGS M48T02 ...)                                          */
/*   Initialized with true time and incremented every simulator second       */
/*   Stores data and time in BCD format                                      */
/*   2 programmable periodic interrupts (1 ms to 10 s interval)              */
/*                                                                           */
/*****************************************************************************/
/*                                                                           */
/* Register Layout:                                                          */
/*   0x00        Status Register        n/a<7:2>  int2<1> int1<0>            */
/*                    read/write - writing bit 1 as 1 clears interrupt       */
/*   0x01        Control Register       n/a<7:2> write read                  */
/*                                                                           */
/*   0x02        Periodic Interrupt 1   n/a<7:5> 10s 1s 100ms 10ms 1ms       */
/*                    setting a bit enables periodic interrupt               */
/*   0x03        Interrupt Vector 1                                          */
/*   0x04        Interrupt Target 1     0xFF - all                           */
/*                                                                           */
/*   0x05        Periodic Interrupt 2   n/a<7:5> 10s 1s 100ms 10ms 1ms       */
/*                    setting a bit enables periodic interrupt               */
/*   0x06        Interrupt Vector 2                                          */
/*   0x07        Interrupt Target 2     0xFF - all                           */
/*                                                                           */
/*   0x08        Year BCD encoded       00-99                                */
/*   0x09        Month BCD encoded      01-12                                */
/*   0x0A        Date BCD encoded       01-31                                */
/*   0x0B        Day of week            01-07                                */
/*   0x0C        Hour BCD encoded       00-23                                */
/*   0x0D        Minutes BCD encoded    00-59                                */
/*   0x0E        Seconds BCD encoded    00-59                                */
/*                                                                           */
/*****************************************************************************/
/*                                                                           */
/* Operation:                                                                */
/*   Read Clock:  set read bit to 1 to disable update of clock registers     */
/*                read clock registers                                       */
/*                clear read bit in control register                         */
/*   Write Clock: set write bit in control register                          */
/*                write clock registers                                      */
/*                clear write bit - clock will get updated                   */
/*   Interrupt:   write interrupt vector                                     */
/*                write interrupt target CPU ID (0xFF = all)                 */
/*                set bit or bits in interrupt register                      */
/*   Handle Int:  read status register to determine interrupt source         */
/*                write bit 1 as 1 to clear interrupt                        */
/*                                                                           */
/*****************************************************************************/





#include <string.h>
#include <malloc.h>
#include <locale.h>
#include <values.h>
#include <time.h>

#include "sim_main/simsys.h"
#include "sim_main/util.h"
#include "Processor/simio.h"
#include "Processor/pagetable.h"
#include "Caches/system.h"
#include "Caches/syscontrol.h"
#include "IO/addr_map.h"
#include "IO/io_generic.h"
#include "IO/realtime_clock.h"

#include "../../lamix/mm/mm.h"
#include "../../lamix/kernel/syscontrol.h"


double RTC_TICK;

REALTIME_CLOCK *RTCs;


void RTC_update(int);
void RTC_set(int);
void RTC_interrupt(int, unsigned*, unsigned char);

time_t rtc_start_time;

struct tm *localtime_r(const time_t*, struct tm*);



/*===========================================================================*/
/* Initialize Realtime Clock                                                 */
/*   Init bus interface, read clock period and calculate 1ms interval        */
/*   register address range and allocate 1 page of memory                    */
/*   clear status and control registers, set time value to current real time */
/*===========================================================================*/

void RTC_init(void)
{
  int             i, v0, v1, v2;
  REALTIME_CLOCK *prtc;
  char            date_str[50], time_str[50];
  struct tm       start;
  
  
  IOGeneric_init(RTC_read, RTC_write, NULL);

  RTC_TICK = 1e9 / CPU_CLK_PERIOD;                 /* clk_period comes in ps */

  RTCs = RSIM_CALLOC(REALTIME_CLOCK, ARCH_numnodes);
  if (!RTCs)
    YS__errmsg(0, "malloc failed at %s:%i", __FILE__, __LINE__);

  /* get local time to initialize start time */
  tzset();
  rtc_start_time = time(NULL);
  localtime_r(&rtc_start_time, &start);
  start.tm_isdst = -1;

  /* check configuration file entries */
  strcpy(time_str, "");
  get_parameter("RTC_start_time", time_str, PARAM_STRING);
  strcpy(date_str, "");
  get_parameter("RTC_start_date", date_str, PARAM_STRING);

  /* convert start time */
  if ((strcasecmp(time_str, "current") != 0) &&
      (sscanf(time_str, "%i:%i:%i", &v0, &v1, &v2) == 3))
    {
      start.tm_hour = v0;
      start.tm_min  = v1;
      start.tm_sec  = v2;
    }

  /* convert start date */
  if ((strcasecmp(date_str, "current") != 0) &&
      (sscanf(date_str, "%i/%i/%i", &v0, &v1, &v2) == 3))
    {
      start.tm_mon  = v0-1;
      start.tm_mday = v1;
      start.tm_year = v2-1900;
    }

  /* no start data and time: use old initial time for backwards compatability*/
  if ((strcasecmp(time_str, "") == 0) &&
      (strcasecmp(date_str, "") == 0))
    rtc_start_time = 0x39BBD9D5;
  /* otherwise use time and date specified, or current */
  else
    rtc_start_time = mktime(&start);
  
  
  /*-------------------------------------------------------------------------*/

  for (i = 0; i < ARCH_numnodes; i++)
    {
      prtc = &(RTCs[i]);

      prtc->nodeid = i;
      prtc->mid    = ARCH_cpus + ARCH_ios;
      
      prtc->old_write_bit = 0;
      prtc->old_status    = 0;
      prtc->clock         = 0;

      prtc->rt            = rtc_start_time;

      AddrMap_insert(i, RTC_BASE_ADDR, RTC_BASE_ADDR + 0x0F,
		     ARCH_cpus + ARCH_ios);
      PageTable_insert_alloc(i, RTC_BASE_ADDR);

      write_char(i, RTC_STATUS,      0);                /* clear status reg  */
      write_char(i, RTC_CONTROL,     0);                /* clear control reg */
      write_char(i, RTC_INT1_CNTL,   0);                /* clear int1 reg    */
      write_char(i, RTC_INT1_VECTOR, 0);                /* clear vector1 reg */
      write_char(i, RTC_INT1_TARGET, 0);                /* clear target1 reg */
      write_char(i, RTC_INT2_CNTL,   0);                /* clear int2 reg    */
      write_char(i, RTC_INT2_VECTOR, 0);                /* clear vector2 reg */
      write_char(i, RTC_INT2_TARGET, 0);                /* clear target2 reg */

      RTC_update(i);

      prtc->update = NewEvent("Realtime clock tick", RTC_event, NODELETE, 0);
      EventSetArg(prtc->update, prtc, sizeof(prtc));
      if ((i >= ARCH_firstnode) && (i < ARCH_firstnode + ARCH_mynodes))
	schedule_event(prtc->update, YS__Simtime + RTC_TICK);

      prtc->intr1_count   = 0;
      prtc->intr1_lat_max = 0;
      prtc->intr1_lat_avg = 0;
      prtc->intr1_lat_min = MAXDOUBLE;
      prtc->intr2_count   = 0;
      prtc->intr2_lat_max = 0;
      prtc->intr2_lat_avg = 0;
      prtc->intr2_lat_min = MAXDOUBLE;
    }

  ARCH_ios++;
}




/*===========================================================================*/
/* Update clock registers (if read-bit is 0) with current time               */
/*===========================================================================*/

#define to_bcd(a) (((a) / 10) << 4) | ((a) % 10)


void RTC_update(int i)
{
  struct tm *t;

  t = gmtime(&(RTCs[i].rt));

  if ((read_bit(i, RTC_BASE_ADDR, 16) == 1) ||
      (read_bit(i, RTC_BASE_ADDR, 17) == 1))
    return;
	
  write_char(i, RTC_YEAR,   to_bcd(t->tm_year));
  write_char(i, RTC_MONTH,  to_bcd(t->tm_mon+1));
  write_char(i, RTC_DATE,   to_bcd(t->tm_mday));
  write_char(i, RTC_DAY,    to_bcd(t->tm_wday+1));
  write_char(i, RTC_HOUR,   to_bcd(t->tm_hour));
  write_char(i, RTC_MINUTE, to_bcd(t->tm_min));
  write_char(i, RTC_SECOND, to_bcd(t->tm_sec));
}


/*===========================================================================*/
/* Set clock with contents of clock registers                                */
/*===========================================================================*/

#define from_bcd(a) (((a) >> 4) * 10 + ((a) & 0x0F))

void RTC_set(int i)
{
  struct tm t;

  t.tm_year  = from_bcd(read_char(i, RTC_YEAR));
  t.tm_mon   = from_bcd(read_char(i, RTC_MONTH) - 1);
  t.tm_mday  = from_bcd(read_char(i, RTC_DATE));
  t.tm_wday  = from_bcd(read_char(i, RTC_DAY) - 1);
  t.tm_hour  = from_bcd(read_char(i, RTC_HOUR));
  t.tm_min   = from_bcd(read_char(i, RTC_MINUTE));
  t.tm_sec   = from_bcd(read_char(i, RTC_SECOND));
  t.tm_isdst = -1;
  
  RTCs[i].rt = mktime(&t);
}



/*===========================================================================*/
/* Read Callback Function - generate reply transaction                       */
/*===========================================================================*/

int RTC_read(REQ* req)
{
  req->type     = REPLY;
  req->req_type = REPLY_UC;

  IO_start_transaction(PID2IO(req->node, req->dest_proc), req);

  return(1);
}


/*===========================================================================*/
/* Write Callback Function - detect writes to 'write' flag to set clock      */
/*===========================================================================*/

int RTC_write(REQ* req)
{
  REALTIME_CLOCK *prtc = &RTCs[req->node];
  int    nid = req->node;
  char   val;
  double lat;

  while (req != NULL)
    { 
      if (req->paddr == RTC_CONTROL)
	{
	  if ((read_bit(nid, RTC_BASE_ADDR, 17) == 0) &&
	      (prtc->old_write_bit == 1))
	    RTC_set(nid);
	  
	  prtc->old_write_bit = read_bit(nid, RTC_BASE_ADDR, 17);
	}

      if (req->paddr == RTC_STATUS)
	{
	  val = read_char(nid, RTC_STATUS);

	  if (val & 0x01)
	    {
	      lat = YS__Simtime - prtc->intr1_start;
	      if (lat > prtc->intr1_lat_max)
		prtc->intr1_lat_max = lat;
	      if (lat < prtc->intr1_lat_min)
		prtc->intr1_lat_min = lat;
	      prtc->intr1_lat_avg += lat;
	    }
	  
	  if (val & 0x02)
	    {
	      lat = YS__Simtime - prtc->intr2_start;
	      if (lat > prtc->intr2_lat_max)
		prtc->intr2_lat_max = lat;
	      if (lat < prtc->intr2_lat_min)
		prtc->intr2_lat_min = lat;
	      prtc->intr2_lat_avg += lat;
	    }
	  
	  write_char(nid, RTC_STATUS,
		     prtc->old_status ^ val);
	  prtc->old_status = read_char(nid, RTC_STATUS);
	}
      
      req = req->parent;
    }

  return(1);
}



/*===========================================================================*/
/* Periodic event function - called every 1 ms (simulator time)              */
/* increment counter, check if a periodic interrupt needs to be generated    */
/* when counter reaches 1000 (1 second), increment and update clock          */
/*===========================================================================*/

void RTC_event(void)
{
  REALTIME_CLOCK *prtc = (REALTIME_CLOCK*)EventGetArg(NULL);

  prtc->clock++;

  /* update clock and clock register after 1000 ms --------------------------*/
  if (prtc->clock % 1000 == 0)
    {
      prtc->rt++;
      RTC_update(prtc->nodeid);
    }

  /* check if need to trigger interrupt 1 -----------------------------------*/

  if ((((prtc->clock % 1     == 0) &&
	read_bit(prtc->nodeid, RTC_BASE_ADDR, 8)) ||
       ((prtc->clock % 10    == 0) &&
	read_bit(prtc->nodeid, RTC_BASE_ADDR, 9)) ||
       ((prtc->clock % 100   == 0) &&
	read_bit(prtc->nodeid, RTC_BASE_ADDR, 10)) ||
       ((prtc->clock % 1000  == 0) &&
	read_bit(prtc->nodeid, RTC_BASE_ADDR, 11)) ||
       ((prtc->clock % 10000 == 0) &&
	read_bit(prtc->nodeid, RTC_BASE_ADDR, 12))) &&
      (!read_bit(prtc->nodeid, RTC_BASE_ADDR, 24)))
    {
      prtc->old_status = 0x01;
      write_bit(prtc->nodeid, RTC_BASE_ADDR, 24, 1);
      prtc->vector1 = (unsigned)read_char(prtc->nodeid, RTC_INT1_VECTOR);
 
      prtc->intr1_count++;
      prtc->intr1_start = YS__Simtime;

      RTC_interrupt(prtc->nodeid,
		    &prtc->vector1,
		    read_char(prtc->nodeid, RTC_INT1_TARGET));
    }
  
  /* check if need to trigger interrupt 2 -----------------------------------*/

  if ((((prtc->clock % 1     == 0) &&
	read_bit(prtc->nodeid, RTC_BASE_ADDR+4, 16)) ||
       ((prtc->clock % 10    == 0) &&
	read_bit(prtc->nodeid, RTC_BASE_ADDR+4, 17)) ||
       ((prtc->clock % 100   == 0) &&
	read_bit(prtc->nodeid, RTC_BASE_ADDR+4, 18)) ||
       ((prtc->clock % 1000  == 0) &&
	read_bit(prtc->nodeid, RTC_BASE_ADDR+4, 19)) ||
       ((prtc->clock % 10000 == 0) &&
	read_bit(prtc->nodeid, RTC_BASE_ADDR+4, 20))) &&
      (!read_bit(prtc->nodeid, RTC_BASE_ADDR, 25)))
    {
      prtc->old_status |= 0x02;
      write_bit(prtc->nodeid, RTC_BASE_ADDR, 25, 1);
      prtc->vector2 = (unsigned)read_char(prtc->nodeid, RTC_INT2_VECTOR);

      prtc->intr2_count++;
      prtc->intr2_start = YS__Simtime;

      RTC_interrupt(prtc->nodeid,
		    &prtc->vector2,
		    read_char(prtc->nodeid, RTC_INT2_TARGET));
    }
  
  /* reschedule only if CPUs are still running ------------------------------*/

  if (prtc->clock % 10000 == 0)
    prtc->clock = 0;

  if (!EXIT)
    schedule_event(prtc->update, YS__Simtime + RTC_TICK);
}





/*****************************************************************************/
/* generate interrupt transaction to target, or to all CPUs                  */
/*****************************************************************************/

void RTC_interrupt(int nodeid, unsigned *vector, unsigned char target)
{
  REQ* req;
  unsigned addr;
  
  if ((target > ARCH_cpus) && (target != 0xFF))
    return;

  req = (REQ *) YS__PoolGetObj(&YS__ReqPool);  
  
  if (target != 0xFF)                 /* single-CPU interrupt */
    addr = SYSCONTROL_THIS_LOW(target) + SC_INTERRUPT;
  else
    addr = SYSCONTROL_LOCAL_LOW + SC_INTERRUPT;

  req->vaddr = addr;
  req->paddr = addr;

  req->size  = 4;
  
  req->d.mem.buf = (unsigned char*)vector;
  req->perform  = IO_write_word;
  req->complete = (void(*)(REQ*, HIT_TYPE))IO_empty_func;
  
  req->node      = nodeid;
  req->src_proc  = RTCs[nodeid].mid;
  req->dest_proc = target;

  req->type = REQUEST;
  req->req_type = WRITE_UC;
  req->prcr_req_type = WRITE;
  req->prefetch = 0;

  req->parent = NULL;
  
  IO_start_transaction(PID2IO(req->node, req->src_proc), req);
}




/*****************************************************************************/
/*****************************************************************************/

void RTC_print_params(int nid)
{
  REALTIME_CLOCK *prtc = &RTCs[nid];
  int             target, vector, mask;
  char            str[40];
  struct tm      *start;

  YS__statmsg(nid, "Realtime Clock Configuration\n");

  target = read_char(nid, RTC_INT1_TARGET);
  vector = read_char(nid, RTC_INT1_VECTOR);
  mask   = read_char(nid, RTC_INT1_CNTL);

  if (target == 0xFF)
    sprintf(str, "Broadcast Interrupt");
  else
    sprintf(str, "Interrupt CPU %i", target);
  
  if (mask != 0)
    {
      if (mask & 0x10)
	YS__statmsg(nid, "  %s every 10 s on IRQ %i\n", str, vector);
      if (mask & 0x08)
	YS__statmsg(nid, "  %s every 1 s on IRQ %i\n", str, vector);
      if (mask & 0x04)
	YS__statmsg(nid, "  %s every 100 ms on IRQ %i\n", str, vector);
      if (mask & 0x02)
	YS__statmsg(nid, "  %s every 10 ms on IRQ %i\n", str, vector);
      if (mask & 0x01)
	YS__statmsg(nid, "  %s every 1 ms on IRQ %i\n", str, vector);
    }


  target = read_char(nid, RTC_INT2_TARGET);
  vector = read_char(nid, RTC_INT2_VECTOR);
  mask   = read_char(nid, RTC_INT2_CNTL);

  if (target == 0xFF)
    sprintf(str, "Broadcast Interrupt");
  else
    sprintf(str, "Interrupt CPU %i", target);
  
  if (mask != 0)
    {
      if (mask & 0x10)
	YS__statmsg(nid, "  %s every 10 s on IRQ %i\n", str, vector);
      if (mask & 0x08)
	YS__statmsg(nid, "  %s every 1 s on IRQ %i\n", str, vector);
      if (mask & 0x04)
	YS__statmsg(nid, "  %s every 100 ms on IRQ %i\n", str, vector);
      if (mask & 0x02)
	YS__statmsg(nid, "  %s every 10 ms on IRQ %i\n", str, vector);
      if (mask & 0x01)
	YS__statmsg(nid, "  %s every 1 ms on IRQ %i\n", str, vector);
    }


  start = localtime(&rtc_start_time);
  YS__statmsg(nid, "  Initial time/date: %d:%02d:%02d %i/%02d/%04d\n",
	      start->tm_hour, start->tm_min, start->tm_sec,
	      start->tm_mon+1, start->tm_mday, start->tm_year + 1900);
  YS__statmsg(nid, "\n");
}



void RTC_stat_report(int nid)
{
  REALTIME_CLOCK *prtc = &RTCs[nid];

  YS__statmsg(nid, "Realtime Clock Statistics\n");

  if (prtc->intr1_count > 0)
    {
      prtc->intr1_lat_avg /= prtc->intr1_count;

      YS__statmsg(nid, "    Interrupt 1:\n");
      YS__statmsg(nid, "        %i Interrupt%s\n",
		  prtc->intr1_count,
		  prtc->intr1_count == 1 ? "" : "s");

      YS__statmsg(nid, "        ");
      PrintTime(prtc->intr1_lat_min * (double)CPU_CLK_PERIOD / 1.0e12,
		statfile[nid]);
      YS__statmsg(nid, " Min Latency\n");
      YS__statmsg(nid, "        ");
      PrintTime(prtc->intr1_lat_avg * (double)CPU_CLK_PERIOD / 1.0e12,
		statfile[nid]);
      YS__statmsg(nid, " Avg Latency\n");
      YS__statmsg(nid, "        ");
      PrintTime(prtc->intr1_lat_max * (double)CPU_CLK_PERIOD / 1.0e12,
		statfile[nid]);
      YS__statmsg(nid, " Max Latency\n");
    }

  
  if (prtc->intr2_count > 0)
    {
      prtc->intr2_lat_avg /= prtc->intr2_count;

      YS__statmsg(nid, "    Interrupt 2:\n");
      YS__statmsg(nid, "        %i Interrupts\n", prtc->intr2_count);
      
      YS__statmsg(nid, "        ");
      PrintTime(prtc->intr2_lat_min * (double)CPU_CLK_PERIOD / 1.0e12,
		statfile[nid]);
      YS__statmsg(nid, " Min Latency\n");
      YS__statmsg(nid, "        ");
      PrintTime(prtc->intr2_lat_avg * (double)CPU_CLK_PERIOD / 1.0e12,
		statfile[nid]);
      YS__statmsg(nid, " Avg Latency\n");
      YS__statmsg(nid, "        ");
      PrintTime(prtc->intr2_lat_max * (double)CPU_CLK_PERIOD / 1.0e12,
		statfile[nid]);
      YS__statmsg(nid, " Max Latency\n");
    }

  YS__statmsg(nid, "\n");
}




void RTC_stat_clear(int nid)
{
  RTCs[nid].intr1_count   = 0;
  RTCs[nid].intr1_lat_max = 0.0;
  RTCs[nid].intr1_lat_min = MAXDOUBLE;
  RTCs[nid].intr1_lat_avg = 0;

  RTCs[nid].intr2_count   = 0;
  RTCs[nid].intr2_lat_max = 0.0;
  RTCs[nid].intr2_lat_min = MAXDOUBLE;
  RTCs[nid].intr2_lat_avg = 0;
}


void RTC_dump(int nid)
{
  REALTIME_CLOCK *prtc = &RTCs[nid];
  IO_GENERIC *pio = PID2IO(nid, prtc->mid);
  
  YS__logmsg(nid, "\n============ REALTIME CLOCK ==============\n");
  YS__logmsg(nid, "clock(%d), rt(%d), module(%d)\n",
	     prtc->clock, prtc->rt, prtc->mid);
  YS__logmsg(nid, "status(0x%x), control(0x%x)\n",
	     read_char(nid, RTC_STATUS), read_char(nid, RTC_CONTROL));
  YS__logmsg(nid, "int1: cntl(0x%x), vector(0x%x), target(0x%x)\n",
	     read_char(nid, RTC_INT1_CNTL),
	     read_char(nid, RTC_INT1_VECTOR),
	     read_char(nid, RTC_INT1_TARGET));
  YS__logmsg(nid, "int2: cntl(0x%x), vector(0x%x), target(0x%x)\n",
	     read_char(nid, RTC_INT2_CNTL),
	     read_char(nid, RTC_INT2_VECTOR),
	     read_char(nid, RTC_INT2_TARGET));
  YS__logmsg(nid, "update scheduled: %s\n",
	     IsScheduled(RTCs[nid].update) ? "yes" : "no");
  IO_dump(pio);
}

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

#include <stdio.h>
#include <strings.h>
#include "Processor/simio.h"
#include "sim_main/util.h"
#include "Caches/system.h"
#include "Caches/cache.h"
#include "Memory/mmc.h"
#include "DRAM/cqueue.h"
#include "DRAM/dram_param.h"
#include "DRAM/dram.h"


/* Report the DRAM backend statistics =======================================*/

void DRAM_stat_report(int nid)
{
  int          k, jtwid, bankid, rd_busid;
  dram_info_t *pdb = NID2DRAM(nid);

#ifdef TRACEDRAM
  if (dparam.trace_on)
    fclose(dram_trace_fp);
#endif

  if (dparam.collect_stats == 0 || Cache_perfect())
    return;
  
  YS__statmsg(nid, "DRAM Backend Statistics\n");


  YS__statmsg(nid, "  Statistics for slave address bus\n");
  YS__statmsg(nid, "    count:               %12lld\t", pdb->sa_bus.count);
  YS__statmsg(nid, "    average cycles:      %12.3f\n",
	      pdb->sa_bus.cycles / pdb->sa_bus.count);
  YS__statmsg(nid, "    waits:               %12lld\n", pdb->sa_bus.waits);
  if (pdb->sa_bus.waits > 0)
    {
      YS__statmsg(nid, "    each wait cycles:    %12.3f\t",
		  pdb->sa_bus.wcycles / pdb->sa_bus.waits);
      YS__statmsg(nid, "    average wait cycles: %12.3f\n",
		  pdb->sa_bus.wcycles / pdb->sa_bus.count);
    }

  
  YS__statmsg(nid, "\n  Statistics for slave data bus\n");
  YS__statmsg(nid, "    count:               %12lld\t", pdb->sd_bus.count);
  YS__statmsg(nid, "    average cycles:      %12.3f\n",
	      pdb->sd_bus.cycles / pdb->sd_bus.count);
  YS__statmsg(nid, "    waits:               %12lld\n", pdb->sd_bus.waits);
  if (pdb->sd_bus.waits > 0)
    {
      YS__statmsg(nid, "    each wait cycles:    %12.3f\t",
		  pdb->sd_bus.wcycles / pdb->sd_bus.waits);
      YS__statmsg(nid, "    average wait cycles: %12.3f\n",
		  pdb->sd_bus.wcycles / pdb->sd_bus.count);
    }

  for (k = 0; k < dparam.rd_busses; k++)
    {
      dram_rd_bus_t *prd = &(pdb->rd_busses[k]);

      YS__statmsg(nid, "\n  Statistics for RD bus %d\n", k);
      YS__statmsg(nid, "    count:               %12lld\t", prd->count);
      YS__statmsg(nid, "    average cycles:      %12.3f\n",
	      prd->cycles / prd->count);
      YS__statmsg(nid, "    waits:               %12lld\n", prd->waits);
      if (prd->waits > 0)
	{
	  YS__statmsg(nid, "    each wait cycles:    %12.3f\t",
		  prd->wcycles/prd->waits);
	  YS__statmsg(nid, "    average wait cycles: %12.3f\n",
		  prd->wcycles/prd->count);
        }
    }

  for (k = 0; k < dparam.num_banks; k++)
    {
      dbank_stat_t *pbstats = &(pdb->banks[k].stats);

      YS__statmsg(nid,
	      "\n  Statistics for bank %d\n", k);
      
      YS__statmsg(nid,
	      "    total reads:         %12lld\n", pbstats->reads);
      YS__statmsg(nid,
	      "    read hits:           %12lld\t", pbstats->read_hits);
      YS__statmsg(nid,
	      "  read hit ratio:      %6.3f\n", 
	      100.0 * pbstats->read_hits / pbstats->reads);
      
      YS__statmsg(nid,
	      "    total writes:        %12lld\n", pbstats->writes);
      YS__statmsg(nid,
	      "    write hits:          %12lld\t", pbstats->write_hits);
      YS__statmsg(nid,
	      "  write hit ratio:     %6.3f\n", 
	      100.0 * pbstats->write_hits / pbstats->writes);
      
      YS__statmsg(nid,
	      "    total writes/reads   %12lld\n", 
	      pbstats->reads + pbstats->writes);
      YS__statmsg(nid,
	      "    total hits:          %12lld\t", 
	      pbstats->read_hits + pbstats->write_hits);
      YS__statmsg(nid,
	      "  total hit ratio:     %6.3f\n",
	      100.0 * (pbstats->read_hits + pbstats->write_hits) / 
	      (pbstats->reads + pbstats->writes));

      /* Statistics about waiting queue on bank */
      YS__statmsg(nid,
	      "    total accesses:      %12lld\n", pbstats->readwrites);
      YS__statmsg(nid,
	      "    average queue length:      %6.3f\n",
	      1.0*pbstats->queue_len / pbstats->readwrites);

      YS__statmsg(nid,
	      "    bank queue waits:    %12lld\t",
	      pbstats->queue_waits);
      YS__statmsg(nid,
	      "  wait ratio:          %6.3f\n",
	      1.0*pbstats->queue_waits / pbstats->readwrites);
      YS__statmsg(nid,
	      "    total cycles:        %12.0f\n", pbstats->queue_cycles);
      YS__statmsg(nid,
	      "    cycles of each wait:       %6.3f\t",
	      1.0*pbstats->queue_cycles / pbstats->queue_waits);
      YS__statmsg(nid,
	      "  average wait cycles: %6.3f\n",
	      1.0*pbstats->queue_cycles / pbstats->readwrites);
      YS__statmsg(nid,
	      "    average service:         %8.0f cycles\n",
	      1.0*pbstats->access_cycles / pbstats->readwrites);
    }

  YS__statmsg(nid, "\n");
}




/*=============================================================================
 * DRAM backend configuration
 */

void DRAM_print_params(int nid)
{
  YS__statmsg(nid, "DRAM Backend Configuration\n");

  if (MMC_sim_on() == 0)
    {
      YS__statmsg(nid, "  DRAMs not simulated\n\n");
      return;
    }  

  if (dparam.sim_on == 0)
    {
      YS__statmsg(nid, "  DRAM simulator     Off  (simulating fixed latency)\n");
      YS__statmsg(nid, "  DRAM latency:     %4d\n", dparam.latency);
      YS__statmsg(nid, "  DRAM statistics:   %s\n\n",
		  dparam.collect_stats ? " On" : "Off");
      return;
    }

  YS__statmsg(nid, "  DRAM frequency      %4d\t", dparam.frequency);
  YS__statmsg(nid, "DRAM scheduler       %s\n",
	  dparam.scheduler_on ? " on" : "off");
  YS__statmsg(nid, "  num_smcs(RD bus)    %4d\t", dparam.rd_busses);
  YS__statmsg(nid, "num_databufs        %4d\n",   dparam.num_databufs);
  YS__statmsg(nid, "  num_banks           %4d\t", dparam.num_banks);
  YS__statmsg(nid, "banks_per_chip      %4d\n",   dparam.banks_per_chip);
  YS__statmsg(nid, "  DRAM type          %s\n", 
	  (dparam.dram_type == RDRAM) ? "RDRAM" : "SDRAM");
  YS__statmsg(nid, "  hot row policy      %4d\t", dparam.hot_row_policy);
  YS__statmsg(nid, "interleaving        %4d\n", dparam.interleaving);
  YS__statmsg(nid, "  row size            %4d\t", dparam.row_size);
  YS__statmsg(nid, "block size          %4d\n",   dparam.block_size);
  YS__statmsg(nid, "  mininum access      %4d\t", dparam.mini_access);
  YS__statmsg(nid, "width               %4d\n\n", dparam.width);

  if (dparam.dram_type == SDRAM)
    {
      YS__statmsg(nid,
		  "SDRAM Configuration\n");
      YS__statmsg(nid,
	      "  tPacket cycles      %4d\t",   dparam.dtime.s.PACKET);
      YS__statmsg(nid,
	      "tCCD cycles         %4d\n",     dparam.dtime.s.CCD);
      YS__statmsg(nid,
	      "  tRRD cycles         %4d\t",   dparam.dtime.s.RRD);
      YS__statmsg(nid,
	      "tRP cycles          %4d\n",     dparam.dtime.s.RP);
      YS__statmsg(nid,
	      "  tRAS cycles         %4d\t",   dparam.dtime.s.RAS);
      YS__statmsg(nid,
	      "tRCD cycles         %4d\n",     dparam.dtime.s.RCD);
      YS__statmsg(nid,
	      "  tAA cycles          %4d\n",   dparam.dtime.s.AA);
      YS__statmsg(nid,
	      "  tDAL cycles         %4d\t",   dparam.dtime.s.DAL);
      YS__statmsg(nid,
	      "tDPL cycles         %4d\n\n", dparam.dtime.s.DPL);
    }
  else
    {
      YS__statmsg(nid,
		  "RDRAM Configuration\n");
      YS__statmsg(nid,
	      "  tPacket cycles      %4d\n", dparam.dtime.r.PACKET);
      YS__statmsg(nid,
	      "  tRC cycles          %4d\t", dparam.dtime.r.RC);
      YS__statmsg(nid,
	      "tRR cycles          %4d\n",   dparam.dtime.r.RR);
      YS__statmsg(nid,
	      "  tRP cycles          %4d\t", dparam.dtime.r.RP);
      YS__statmsg(nid,
	      "tCBUB1 cycles       %4d\n",   dparam.dtime.r.CBUB1);
      YS__statmsg(nid,
	      "  tCBUB2 cycles       %4d\t", dparam.dtime.r.CBUB2);
      YS__statmsg(nid,
	      "tRCD cycles         %4d\n",   dparam.dtime.r.RCD);
      YS__statmsg(nid,
	      "  tCAC cycles         %4d\t", dparam.dtime.r.CAC);
      YS__statmsg(nid,
	      "tCWD cycles         %4d\n\n", dparam.dtime.r.CWD);
    }
  
  YS__statmsg(nid, "  row hold_time   %8d\n",     dparam.row_hold_time);
  YS__statmsg(nid, "  refresh_delay   %8d\t",     dparam.refresh_delay);
  YS__statmsg(nid, "refresh_period  %8d\n",       dparam.refresh_period);
  YS__statmsg(nid, "  SA bus cycle        %4d\t", dparam.sa_bus_cycles);
  YS__statmsg(nid, "SD bus cycle        %4d\n\n", dparam.sd_bus_cycles);
}




/*=============================================================================
 * Reset the statistics
 */
void DRAM_stat_clear(int nid)
{
  int k;
  dram_info_t *pdb = NID2DRAM(nid);


  if (!dparam.collect_stats)
    return;

  pdb->total_count  = 0;
  pdb->total_cycles = 0;
  pdb->sgle_count   = 0;
  pdb->sgle_cycles  = 0;

  if (dparam.sim_on == 0)
    return;

  pdb->sa_bus.count   = 0;
  pdb->sa_bus.waits   = 0;
  pdb->sa_bus.cycles  = 0;
  pdb->sa_bus.wcycles = 0;
      
  pdb->sd_bus.count   = 0;
  pdb->sd_bus.waits   = 0;
  pdb->sd_bus.cycles  = 0;
  pdb->sd_bus.wcycles = 0;
      
  for (k = 0; k < dparam.rd_busses; k++)
    {
      dram_rd_bus_t *prdbus = &(pdb->rd_busses[k]);
      prdbus->count = 0;
      prdbus->waits = 0;
      prdbus->cycles = 0;
      prdbus->wcycles = 0;
    }

  for (k = 0; k < dparam.num_banks; k++)
    bzero((char *)&(pdb->banks[k].stats), sizeof(dbank_stat_t));
}


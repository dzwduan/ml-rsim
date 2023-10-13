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
#include <stdlib.h>
#include <math.h>
#include <values.h>

#include "sim_main/simsys.h"
#include "sim_main/util.h"
#include "Processor/simio.h"
#include "Caches/system.h"

#include "IO/scsi_disk.h"


double DISK_time_next_sector(SCSI_DISK*, double);



/*=========================================================================*/
/* Determine the sector under a given head at time X based on number of    */
/* rotations since start, head, current band, sector-skew and cylinder-skew*/
/*=========================================================================*/

int DISK_sector_at_time(SCSI_DISK *pdisk, int head, int cylinder,
			double time)
{
  int     base, skew, offset;
  double  angle, rot;

  /* sector at angle 0 - start angle */
  base = (pdisk->heads * pdisk->sectors * cylinder) +
    (head * pdisk->sectors);

  skew = (pdisk->heads * pdisk->track_skew * cylinder) +
    (cylinder * pdisk->cylinder_skew) +
    (head * pdisk->track_skew) + pdisk->start_offset;

  skew = skew % pdisk->sectors;
  
  angle = time * CPU_CLK_PERIOD / 1e6;       /* time in usecs              */
  rot = 1e6 * 60 / pdisk->rpm;               /* time for 1 rotation        */
  angle = fmod(angle, rot);                  /* time into current rotation */
  angle = angle / rot;                       /* ratio into rotation        */

  offset = (int)floor(angle * (double)pdisk->sectors);
  offset = (offset + pdisk->sectors - skew) % pdisk->sectors;

  return(base + offset);
}



/*=========================================================================*/
/* Calculate time to reach beginning of next sector.                       */
/*=========================================================================*/

double DISK_time_next_sector(SCSI_DISK *pdisk, double time)
{
  double angle, rot;

  angle = time * CPU_CLK_PERIOD / 1e6;       /* time in usecs              */
  rot = 1e6 * 60 / pdisk->rpm;               /* time for 1 rotation        */
  angle = fmod(angle, rot);                  /* time into current rotation */
  angle = angle / rot;

  angle = (ceil(angle*(double)pdisk->sectors)/(double)pdisk->sectors) - angle;

  return(angle * rot / 1000);
}




/*=========================================================================*/
/* Convert sector number to cylinder number and head number                */
/*=========================================================================*/

int DISK_sector_to_cylinder(SCSI_DISK *pdisk, int sector)
{
  return(sector / (pdisk->heads * pdisk->sectors));
}


int DISK_sector_to_head(SCSI_DISK *pdisk, int sector)
{
  int cylinder;
  cylinder = sector / (pdisk->heads*pdisk->sectors);
  return((sector - (cylinder * pdisk->heads*pdisk->sectors)) / pdisk->sectors);
}




/*=========================================================================*/
/* Calculate seek time as sum of arm movement time, write settle time (in  */
/* case of a write and an arm movement or a head switch). For small        */
/* movements, the head switch time might be larger, in this case return    */
/* the head switch time.                                                   */
/*=========================================================================*/

double DISK_seek_time(SCSI_DISK *pdisk, int cylinders,
		      int head_switch, int write)
{
  double a, b, c, seek;
  int cyls = abs(cylinders);

  if (pdisk->seek_method == DISK_SEEK_NONE)
    return(0.0);

  
  if (pdisk->seek_method == DISK_SEEK_CONST)
    return(pdisk->seek_avg);


  if (pdisk->seek_method == DISK_SEEK_CURVE)
    {
      if (cyls == 0)
	seek = 0.0;
      else
	{
	  a = (-10.0 * pdisk->seek_single +
 	        15.0 * pdisk->seek_avg -
	         5.0 * pdisk->seek_full) / (3.0 * sqrt(pdisk->cylinders));
	  b = ( 7.0 * pdisk->seek_single -
	       15.0 * pdisk->seek_avg +
		8 * pdisk->seek_full) / (3.0 * pdisk->cylinders);
	  c = pdisk->seek_single;

	  seek = a * sqrt(cyls - 1) + b * (cyls - 1) + c;
        }
    }


  if (pdisk->seek_method == DISK_SEEK_LINE)
    {
      if (cyls == 0)
	seek = 0.0;
      else if (cyls == 1)
	seek = pdisk->seek_single;
      else if (cyls <= pdisk->cylinders / 3)
	{
	  seek = ((cyls - 1.0) / (pdisk->cylinders / 3.0)) *
	    (pdisk->seek_avg - pdisk->seek_single) +
	    pdisk->seek_single;
	}
      else
	{
	  seek = (((cyls * 3.0) / (pdisk->cylinders * 2.0)) - 0.5) *
	    (pdisk->seek_full - pdisk->seek_avg) +
	    pdisk->seek_avg;
	}
    }

  if (write && ((cyls > 0) || head_switch))
    seek += pdisk->write_settle;

  if (head_switch && (seek < pdisk->head_switch))
    seek = pdisk->head_switch;

  return(seek);
}




/*=========================================================================*/
/* Compute actual seek distance based on time.                             */
/*=========================================================================*/

int DISK_seek_distance(SCSI_DISK *pdisk, double time, int total)
{
  double a, b, c, u0, u1;
  int cyls;

  if (pdisk->seek_method == DISK_SEEK_NONE)
    cyls = abs(total);

  
  if (pdisk->seek_method == DISK_SEEK_CONST)
    cyls = (int)floor(abs(total) * (time / pdisk->seek_avg));


  
  if (pdisk->seek_method == DISK_SEEK_CURVE)
    {
      a = (-10.0 * pdisk->seek_single +
	   15.0 * pdisk->seek_avg -
	   5.0 * pdisk->seek_full) / (3.0 * sqrt(pdisk->cylinders));
      b = ( 7.0 * pdisk->seek_single -
	    15.0 * pdisk->seek_avg +
	    8 * pdisk->seek_full) / (3.0 * pdisk->cylinders);
      c = pdisk->seek_single;

      if (time == 0.0)
	cyls = 0;
      else
	{
	  u0 = (-a + sqrt(a * a - 4 * b * (c - time))) / (2 * b);
	  u1 = (-a - sqrt(a * a - 4 * b * (c - time))) / (2 * b);
	  if (u0 > 0)
	    cyls = (int)rint((u0 * u0) - 1);
	  else
	    cyls = (int)rint((u1 * u1) - 1);

	  if (cyls < 0)
	    cyls = 0;
	}
    }

  
  if (pdisk->seek_method == DISK_SEEK_LINE)
    {
      if (time == 0.0)
	cyls = 0;
      else if (time < pdisk->seek_single)
	cyls = 0;
      else if (time < ((pdisk->cylinders/3.0) - 1) / (pdisk->cylinders/3.0) *
	       (pdisk->seek_avg - pdisk->seek_single) +
	       pdisk->seek_single)
	cyls = (time - pdisk->seek_single) * (pdisk->cylinders / 3) /
	  (pdisk->seek_avg - pdisk->seek_single) + 1;
      else
	cyls = ((time - pdisk->seek_avg) /
		(pdisk->seek_full - pdisk->seek_avg) + 0.5) *
	  (pdisk->cylinders * 2 / 3);
    }

  if (cyls > total)
    cyls = total;
  
  return(cyls);
}






/*=========================================================================*/
/* return time to rotate past N sectors.                                   */
/*=========================================================================*/

double DISK_rotation_time(SCSI_DISK *pdisk, int sectors)
{
  int sects;

  sects = sectors;
  if (sectors < 0)
    sects += pdisk->sectors;

  return((60000.0 / (double)pdisk->rpm) *
	 (double)sects / (double)pdisk->sectors);
}




/*=========================================================================*/
/* Estimate time to transfers N sectors. Return rotational time if no seek */
/* is necessary, otherwise return MAXFLOAT.                                */
/*=========================================================================*/

double DISK_estimate_access_time(SCSI_DISK *pdisk, int sector, int length)
{
  int head, cylinder;

  head     = DISK_sector_to_head(pdisk, sector + length - 1);
  cylinder = DISK_sector_to_cylinder(pdisk, sector + length - 1);

  if ((pdisk->current_head == head) && (pdisk->current_cylinder == cylinder))
    return((double)(length * 60000.0 / (double)(pdisk->rpm * pdisk->sectors)));
  else
    return(MAXFLOAT);
}




/*=========================================================================*/
/* Perform seek operation. Compute seek time and determine sector under    */
/* the head when seek is done, add rotation delay to reach target sector.  */
/* Schedule seek-event, set disk state to seek and update current          */
/* cylinder/head.                                                          */
/*=========================================================================*/

void DISK_do_seek(SCSI_DISK *pdisk, int sector, int write)
{
  int         next_head, next_cylinder, sector_hit;
  double      seek, rotation;

  next_cylinder = DISK_sector_to_cylinder(pdisk, sector);
  next_head     = DISK_sector_to_head(pdisk, sector);

  if (IsScheduled(pdisk->seek_event) || IsScheduled(pdisk->sector_event))
    {
      if (sector == pdisk->seek_target_sector)
	{
#ifdef SCSI_DISK_TRACE
	  YS__logmsg(pdisk->scsi_me->scsi_bus->node_id,
		     "[%i:%i] Interrupt? Keep seeking -> %i\n",
		     pdisk->scsi_me->scsi_bus->bus_id+1,
		     pdisk->scsi_me->dev_id,
		     pdisk->seek_target_sector);
#endif
	  
          return;
	}
      else
	{
	  if (pdisk->current_cylinder > pdisk->previous_cylinder)
	    pdisk->current_cylinder = pdisk->previous_cylinder + 
	      DISK_seek_distance(pdisk,
				 (YS__Simtime - pdisk->seek_start_time) / pdisk->ticks_per_ms,
				 pdisk->current_cylinder - pdisk->previous_cylinder);
	  
	  else if (pdisk->current_cylinder < pdisk->previous_cylinder)
	    pdisk->current_cylinder = pdisk->previous_cylinder -
	      DISK_seek_distance(pdisk,
				 (YS__Simtime - pdisk->seek_start_time) / pdisk->ticks_per_ms,
				 pdisk->previous_cylinder - pdisk->current_cylinder);

#ifdef SCSI_DISK_TRACE
	  YS__logmsg(pdisk->scsi_me->scsi_bus->node_id,
		     "[%i:%i] Interrupt seek, current position %i\n",
		     pdisk->scsi_me->scsi_bus->bus_id+1,
		     pdisk->scsi_me->dev_id,
		     pdisk->current_cylinder);
#endif
	  
          pdisk->seek_time -=
	    (pdisk->seek_end_time - YS__Simtime) / pdisk->ticks_per_ms;

	  if (IsScheduled(pdisk->seek_event))
	    deschedule_event(pdisk->seek_event);

	  if (IsScheduled(pdisk->sector_event))
	    deschedule_event(pdisk->sector_event);
	}
    }


  seek = DISK_seek_time(pdisk, next_cylinder - pdisk->current_cylinder,
			next_head != pdisk->current_head, write);

  sector_hit = DISK_sector_at_time(pdisk, next_head, next_cylinder,
				   YS__Simtime + seek * pdisk->ticks_per_ms);

  if (DISK_sector_at_time(pdisk, next_head, next_cylinder,
			  YS__Simtime + seek * pdisk->ticks_per_ms) == sector)
    rotation = 0.0;
  else
    rotation = DISK_rotation_time(pdisk, sector - sector_hit) +
      DISK_time_next_sector(pdisk, YS__Simtime + seek*pdisk->ticks_per_ms);


#ifdef SCSI_DISK_TRACE
  YS__logmsg(pdisk->scsi_me->scsi_bus->node_id,
	     "[%i:%i] Seek %i -> %i, %i -> %i; Current %i, Hit %i, target %i (%i)\n",
	     pdisk->scsi_me->scsi_bus->bus_id+1,
	     pdisk->scsi_me->dev_id,
	     pdisk->current_cylinder, next_cylinder,
	     pdisk->current_head, next_head,
	     DISK_sector_at_time(pdisk,
				 pdisk->current_head,
				 pdisk->current_cylinder,
				 YS__Simtime),
	     sector_hit,
	     sector,
	     DISK_sector_at_time(pdisk,
				 next_head,
				 next_cylinder,
				 YS__Simtime+(seek+rotation)*pdisk->ticks_per_ms));

  YS__logmsg(pdisk->scsi_me->scsi_bus->node_id,
	     "[%i:%i] Wait %f ms -> %f\n",
	     pdisk->scsi_me->scsi_bus->bus_id+1,
	     pdisk->scsi_me->dev_id,
	     seek + rotation,
	     floor((seek + rotation) * pdisk->ticks_per_ms));
#endif
  
  if (IsNotScheduled(pdisk->seek_event))
    schedule_event(pdisk->seek_event,
		   YS__Simtime + floor((seek + rotation)*pdisk->ticks_per_ms));

  pdisk->seek_start_time    = YS__Simtime;
  pdisk->seek_end_time      = YS__Simtime +
    floor((seek + rotation)*pdisk->ticks_per_ms);
  pdisk->previous_cylinder  = pdisk->current_cylinder;
  pdisk->seek_target_sector = sector;

  pdisk->state              = DISK_SEEK;
  pdisk->current_head       = next_head;
  pdisk->current_cylinder   = next_cylinder;
  pdisk->seek_time         += seek + rotation;
}





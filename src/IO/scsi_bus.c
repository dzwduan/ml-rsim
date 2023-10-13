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

/*---------------------------------------------------------------------------*/

#include <stdio.h>
#include <stdlib.h>

#include "sim_main/simsys.h"
#include "sim_main/util.h"
#include "Processor/simio.h"
#include "Caches/system.h"
#include "IO/scsi_bus.h"




int SCSI_WIDTH       = 2;     /* 16 byte SCSI data path                      */
int SCSI_FREQUENCY   = 10;    /* SCSI bus frequency in MHz                   */
int SCSI_FREQ_RATIO;          /* CPU to SCSI frequency ratio                 */
int SCSI_ARB_DELAY   = 24;    /* arbitration delay in SCSI cycles - 2.4 us   */
int SCSI_BUS_FREE    = 8;     /* minimum bus free time in cycles - 800 ns    */
int SCSI_REQ_DELAY   = 13;    /* lumped delay for request transfer in cycles */
int SCSI_SEL_TIMEOUT = 10000; /* target selection timeout in cycles - 1 ms   */


const char *SCSI_ReqName[SCSI_REQ_MAX] =
{
  "MISCELLANEOUS",
  "INQUIRY      ",
  "MODE SENSE   ",
  "READ CAPACITY",
  "REQUEST SENSE",
  "READ         ",
  "WRITE        ",
  "PREFETCH     ",
  "SEEK         ",
  "FORMAT       ",
  "SYNC_CACHE   ",
  "RECONNECT    ",
};


const char *SCSI_RepName[SCSI_REP_MAX] =
{
  "COMPLETE  ",
  "CONNECT   ",
  "DISCONNECT",
  "SAVE_D_PTR",
  "BUSY      ",
  "REJECT    ",
  "TIMEOUT   "
};


void SCSI_bus_arbitrate();
void SCSI_bus_send_request();
void SCSI_bus_send_response();



/*===========================================================================*/
/* Initialize SCSI subsystem - read parameters                               */
/* Should be called before any other SCSI devices are initialized.           */
/*===========================================================================*/

void SCSI_init(void)
{
  /* note that SCSI_frequency is specified in MHz, not as a ratio.           */
  /* various delays and latencies are given in SCSI cycles.                  */
  get_parameter("SCSI_width",      &SCSI_WIDTH,        PARAM_INT);
  get_parameter("SCSI_frequency",  &SCSI_FREQUENCY,    PARAM_INT);
  get_parameter("SCSI_arb_delay",  &SCSI_ARB_DELAY,    PARAM_INT);
  get_parameter("SCSI_bus_free",   &SCSI_BUS_FREE,     PARAM_INT);
  get_parameter("SCSI_req_delay",  &SCSI_REQ_DELAY,    PARAM_INT);
  get_parameter("SCSI_timeout",    &SCSI_SEL_TIMEOUT,  PARAM_INT);

  SCSI_FREQ_RATIO = (1000000 / SCSI_FREQUENCY) / CPU_CLK_PERIOD;
}



/*===========================================================================*/
/* Initialize a SCSI bus                                                     */
/* Allocate memory for bus control structure, initialize it, allocate        */
/* space for N attached devices and create the events.                       */
/*===========================================================================*/

SCSI_BUS *SCSI_bus_init(int node_id, int bus_id)
{
  SCSI_BUS *psbus;
  int       n;
  
  psbus = (SCSI_BUS*)malloc(sizeof(SCSI_BUS));
  if (psbus == NULL)
    YS__errmsg(node_id, "Malloc failed at %s:%i", __FILE__, __LINE__);

  psbus->node_id = node_id;
  psbus->bus_id  = bus_id;
  psbus->state   = SCSI_BUS_IDLE;


  psbus->current_req = NULL;
  psbus->devices = (SCSI_DEV**)malloc(SCSI_WIDTH * 8 * sizeof(SCSI_DEV*));
  if (psbus->devices == NULL)
    YS__errmsg(node_id, "Malloc failed at %s:%i", __FILE__, __LINE__);

  for (n = 0; n < SCSI_WIDTH * 8; n++)
    psbus->devices[n] = NULL;


  psbus->arbitrate = NewEvent("SCSI Bus Arbitrate",
			      SCSI_bus_arbitrate, NODELETE, 0);
  EventSetArg(psbus->arbitrate, (char*)psbus, sizeof(psbus));

  psbus->send_request = NewEvent("SCSI Bus Send Request",
				 SCSI_bus_send_request, NODELETE, 0);
  EventSetArg(psbus->send_request, (char*)psbus, sizeof(psbus));

  psbus->send_response = NewEvent("SCSI Bus Send Response",
				  SCSI_bus_send_response, NODELETE, 0);
  EventSetArg(psbus->send_response, (char*)psbus, sizeof(psbus));

  
  psbus->utilization = 0.0;
  psbus->num_trans = 0;
  psbus->last_clear = 0.0;
  
  return(psbus);
}




/*===========================================================================*/
/* Called by the arbitrate-event, which is scheduled when a device           */
/* arbitrates and the bus is currently idle, or at the end of a transfer.    */
/* Arbitrate among SCSI devices. Search through list of devices for non-NULL */
/* req-pointer, starting at device N-1. First request becomes current.       */
/* Schedule request event after REQ_DELAY cycles. Writes are delivered       */
/* even later to account for the data transfer.                              */
/*===========================================================================*/

void SCSI_bus_arbitrate()
{
  SCSI_BUS *psbus = (SCSI_BUS*)EventGetArg(NULL);
  int       n;
  int       delay;


  for (n = SCSI_WIDTH * 8 - 1; n >= 0; n--)
    {
      if (psbus->devices[n] == NULL)
	continue;
      
      if (psbus->devices[n]->req != NULL)
	{
	  SCSI_REQ *req;
	  
	  psbus->state = SCSI_BUS_BUSY;
	  req = psbus->current_req = psbus->devices[n]->req;

	  delay = 0;

          if (req->request_type != SCSI_REQ_RECONNECT)
	    {
	      delay = SCSI_REQ_DELAY;
	      schedule_event(psbus->send_request,
			     YS__Simtime + delay * SCSI_FREQ_RATIO);
	    }
	  else
	    {
	      SCSI_REPLY_TYPE old = req->reply_type;

	      /* check if initiator device exists ---------------------------*/
	      if ((req->initiator < 0) ||
		  (req->initiator >= SCSI_WIDTH * 8))
		YS__errmsg(psbus->node_id,
			   "SCSI %i.%i: Illegal initiator ID in request at %.1f\n",
			   psbus->node_id, psbus->bus_id, YS__Simtime);

	      if ((psbus->devices[req->initiator] == NULL) ||
		  (psbus->devices[req->initiator]->response_callback == NULL))
		YS__errmsg(psbus->node_id,
			   "SCSI %i.%i: No response callback registered for module %i\n",
			   psbus->node_id, psbus->bus_id, req->initiator);


	      /* deliver RECONNECT to initiator -----------------------------*/

	      req->reply_type = SCSI_REP_CONNECT;
	      psbus->devices[req->initiator]->response_callback(psbus->devices[req->initiator]->device, req);
	      req->reply_type = old;
	      delay = psbus->devices[n]->req->buscycles;

	      schedule_event(psbus->send_response,
			     YS__Simtime + delay * SCSI_FREQ_RATIO);
	    }

	  psbus->devices[n]->req = NULL;

	  if (psbus->devices[n]->wonbus_callback != NULL)
	    psbus->devices[n]->wonbus_callback(psbus->devices[n]->device,
					       psbus->current_req);

	  return;
	}
    }

  psbus->state = SCSI_BUS_IDLE;
}





/*===========================================================================*/
/* Send a SCSI request to the target. Called by send_request event, which is */
/* scheduled by the arbitration routine to occur at the end of the command   */
/* transfer (including write-data).                                          */
/* Schedule a timeout response after TIMEOUT cycles if target device does    */
/* not exist, or call the devices request callback routine. The callback     */
/* function modifies the request into a response and returns the number of   */
/* cycles after which the response should be returned.                       */
/*===========================================================================*/

void SCSI_bus_send_request()
{
  SCSI_BUS        *psbus = (SCSI_BUS*)EventGetArg(NULL);
  SCSI_REQ        *req = psbus->current_req;
  SCSI_REPLY_TYPE  old;
  int              delay;

  
  /* check if target device exists, timeout if not --------------------------*/
  if ((req->target < 0) ||
      (req->target >= SCSI_WIDTH * 8))
    YS__errmsg(psbus->node_id,
	       "SCSI %i:%i: Illegal target ID in request at %.1f\n",
	       psbus->node_id, psbus->bus_id, YS__Simtime);

  if ((psbus->devices[req->target] == NULL) ||
      (psbus->devices[req->target]->request_callback == NULL))
    {
      req->reply_type = SCSI_REP_TIMEOUT;
      if (IsNotScheduled(psbus->send_response))
	schedule_event(psbus->send_response,
		       YS__Simtime + SCSI_SEL_TIMEOUT * SCSI_FREQ_RATIO);
      return;
    }


  
  /* deliver request to target device, schedule response --------------------*/

  psbus->devices[req->target]->request_callback(psbus->devices[req->target]->device, req);

  
  /* if request is complete and data is to be transferred, send CONNECT to
     initiator */

  if (((req->reply_type == SCSI_REP_COMPLETE) ||
       (req->reply_type == SCSI_REP_SAVE_DATA_POINTER)) &&
      (req->buscycles > 0))
    {
      old = req->reply_type;
      req->reply_type = SCSI_REP_CONNECT;

      /* check if initiator device exists -----------------------------------*/
      if ((req->initiator < 0) ||
	  (req->initiator >= SCSI_WIDTH * 8))
	YS__errmsg(psbus->node_id,
		   "SCSI %i.%i: Illegal initiator ID in request at %.1f\n",
		   psbus->node_id, psbus->bus_id, YS__Simtime);

      if ((psbus->devices[req->initiator] == NULL) ||
	  (psbus->devices[req->initiator]->response_callback == NULL))
	YS__errmsg(psbus->node_id,
		   "SCSI %i.%i: No response callback registered for module %i\n",
		   psbus->node_id, psbus->bus_id, req->initiator);

      
      /* deliver response to initiator --------------------------------------*/

      psbus->devices[req->initiator]->response_callback(psbus->devices[req->initiator]->device, req);

      req->reply_type = old;
    }


  delay = req->buscycles;
  psbus->utilization += delay * SCSI_FREQ_RATIO;

  if (IsNotScheduled(psbus->send_response))
    schedule_event(psbus->send_response,
		   YS__Simtime + delay * SCSI_FREQ_RATIO);
}





/*===========================================================================*/
/* Deliver SCSI Response to initiator. Called by send_response event which   */
/* is scheduled by the request routine. Call the initiators response         */
/* callback function and schedule next arbitration event after BUS_FREE +    */
/* ARB_DELAY cycles.                                                         */
/*===========================================================================*/

void SCSI_bus_send_response()
{
  SCSI_BUS *psbus = (SCSI_BUS*)EventGetArg(NULL);
  SCSI_REQ *req = psbus->current_req;


  psbus->num_trans++;

  /* check if initiator device exists ---------------------------------------*/
  if ((req->initiator < 0) ||
      (req->initiator >= SCSI_WIDTH * 8))
    YS__errmsg(psbus->node_id,
	       "SCSI %i.%i: Illegal initiator ID in request at %.1f\n",
	       psbus->node_id, psbus->bus_id, YS__Simtime);

  if ((psbus->devices[req->initiator] == NULL) ||
      (psbus->devices[req->initiator]->response_callback == NULL))
    YS__errmsg(psbus->node_id,
	       "SCSI %i.%i: No response callback registered for module %i\n",
	       psbus->node_id, psbus->bus_id, req->initiator);


  /* deliver response to initiator ------------------------------------------*/
      
  psbus->devices[req->initiator]->response_callback(psbus->devices[req->initiator]->device, req);

  schedule_event(psbus->arbitrate,
		 YS__Simtime +
		 (SCSI_ARB_DELAY + SCSI_BUS_FREE) * SCSI_FREQ_RATIO);
}





/*===========================================================================*/
/* Create a generic SCSI device on a particular bus.                         */
/* Takes the bus pointer, device ID and a pointer to device-specific storage */
/* and the request/response/statistics callback functions as arguments.      */
/* Creates a device structure in the buses device list and initializes it.   */
/*===========================================================================*/

SCSI_DEV *SCSI_device_init(SCSI_BUS *psbus, int device_id,
			   void *device,
			   void (*wonbus_callback)(void*, SCSI_REQ*),
			   void (*request_callback)(void*, SCSI_REQ*),
			   void (*response_callback)(void*, SCSI_REQ*),
			   void (*perform_callback)(void*, SCSI_REQ*),
			   void (*stat_report)(void*),
			   void (*stat_clear)(void*),
			   void (*dump)(void*))
{
  SCSI_DEV *scsi_dev;
  
  if (psbus->devices == NULL)
    YS__errmsg(psbus->node_id,
	       "SCSI Bus must be initialized before adding devices");

  if ((device_id < 0) || (device_id >= SCSI_WIDTH * 8))
    YS__errmsg(psbus->node_id,
	       "SCSI %i:%i: Attempt to register illegal device ID %i\n",
	       psbus->node_id, psbus->bus_id, device_id);
  
  if (psbus->devices[device_id] != NULL)
    YS__errmsg(psbus->node_id,
	       "SCSI %i:%i: Device %i already registered\n",
	       psbus->node_id, psbus->bus_id, device_id);

  scsi_dev = (SCSI_DEV*)malloc(sizeof(SCSI_DEV));
  if (scsi_dev == NULL)
    YS__errmsg(psbus->node_id,
	       "Malloc failed at %s:%i", __FILE__, __LINE__);

  psbus->devices[device_id] = scsi_dev;
  psbus->devices[device_id]->device            = device;
  psbus->devices[device_id]->dev_id            = device_id;
  psbus->devices[device_id]->req               = NULL;
  psbus->devices[device_id]->wonbus_callback   = wonbus_callback;
  psbus->devices[device_id]->request_callback  = request_callback;
  psbus->devices[device_id]->response_callback = response_callback;
  psbus->devices[device_id]->perform_callback  = perform_callback;
  psbus->devices[device_id]->stat_report       = stat_report;
  psbus->devices[device_id]->stat_clear        = stat_clear;
  psbus->devices[device_id]->dump              = dump;
  psbus->devices[device_id]->scsi_bus = psbus;

  return(scsi_dev);
}




/*===========================================================================*/
/* A SCSI device issues a request: insert request in devices req pointer.    */
/* If bus is idle (not arbitrating or busy), set state to arbitrate and      */
/* schedule arbitration event. Setting the state prevents the next requestor */
/* from scheduling the event again. If the bus is not idle, no event is      */
/* scheduled as this will be done at the end of the current transfer.        */
/*===========================================================================*/

int SCSI_device_request(SCSI_DEV *me, SCSI_REQ *req)
{
  if (me->req != NULL)
    return(0);

  me->req = req;

  if (me->scsi_bus->state == SCSI_BUS_IDLE)
    {
      me->scsi_bus->state = SCSI_BUS_ARBITRATE;
      schedule_event(me->scsi_bus->arbitrate,
		     YS__Simtime + SCSI_ARB_DELAY * SCSI_FREQ_RATIO);
    }

  return(1);
}




/*===========================================================================*/
/* Perform a SCSI Request - call the callback functions for read or write    */
/*===========================================================================*/

void SCSI_bus_perform(SCSI_BUS *pbus, SCSI_REQ *req)
{
  if (((req->orig_request == SCSI_REQ_READ) ||
       (req->orig_request == SCSI_REQ_WRITE)) &&
      (pbus->devices[req->target]->perform_callback != NULL))
	pbus->devices[req->target]->perform_callback(pbus->devices[req->target], req);
}





/*===========================================================================*/
/* Print SCSI bus parameters.                                                */
/*===========================================================================*/

void SCSI_bus_print_params(int nid)
{
  double scsi_cycle;
  
  scsi_cycle = (double)(1.0 / SCSI_FREQUENCY);

  YS__statmsg(nid, "SCSI Bus Configuration\n");
  YS__statmsg(nid, "  Number of devices:\t%5i\n", SCSI_WIDTH * 8);
  YS__statmsg(nid, "  Width:\t\t%5i\t\t  Frequency:\t\t%8.2f MHz\n",
	      SCSI_WIDTH * 8, (float)SCSI_FREQUENCY);
  YS__statmsg(nid, "  Arbitration Delay:\t%8.2f us\t",
	      SCSI_ARB_DELAY * scsi_cycle);
  YS__statmsg(nid, "  Bus Free Delay:\t%8.2f us\n",
	      SCSI_BUS_FREE * scsi_cycle);
  YS__statmsg(nid, "  Request Delay:\t%8.2f us\t",
	      SCSI_REQ_DELAY * scsi_cycle);
  YS__statmsg(nid, "  Select Timeout:\t%8.2f us\n\n",
	      SCSI_SEL_TIMEOUT * scsi_cycle);
}



/*===========================================================================*/
/* Print SCSI bus statistics, and call the statistics function for all       */
/* attached devices, if one exists.                                          */
/*===========================================================================*/

void SCSI_bus_stat_report(SCSI_BUS *psbus)
{
  int n;
  
  YS__statmsg(psbus->node_id,
	      "  Utilization: %.3f%%\t%lld transaction%c\n\n",
	      (100.0 * (double)psbus->utilization) / (YS__Simtime - psbus->last_clear),
	      psbus->num_trans,
	      psbus->num_trans == 1 ? ' ' : 's');
  
  for (n = 0; n < SCSI_WIDTH * 8; n++)
    if ((psbus->devices[n] != NULL) &&
	(psbus->devices[n]->stat_report != NULL))
      psbus->devices[n]->stat_report(psbus->devices[n]->device);
}



/*===========================================================================*/
/* Clear bus statistics and call the the clear_stat function for all         */
/* attached devices.                                                         */
/*===========================================================================*/

void SCSI_bus_stat_clear(SCSI_BUS *psbus)
{
  int n;
  
  psbus->utilization = 0.0;
  psbus->num_trans = 0;
  psbus->last_clear = YS__Simtime;

  for (n = 0; n < SCSI_WIDTH * 8; n++)
    if ((psbus->devices[n] != NULL) &&
	(psbus->devices[n]->stat_clear != NULL))
      psbus->devices[n]->stat_clear(psbus->devices[n]->device);
}



/*===========================================================================*/
/* Dump SCSI bus debug information                                           */
/*===========================================================================*/

void SCSI_req_dump(SCSI_REQ *req, int flags, int nid)
{
  YS__logmsg(nid, "  request_type(%s), reply_type(%s)\n",
             SCSI_ReqName[req->request_type], SCSI_RepName[req->reply_type]);
  YS__logmsg(nid, "  initiator(%d), target(%d), lun(%d)\n", 
             req->initiator, req->target, req->lun);
  YS__logmsg(nid, "  lba(%d), length(%d), imm_flag(%d)\n",
             req->lba, req->length, req->imm_flag);
  YS__logmsg(nid, "  queue_msg(%d), queue_tag(%d), aux(%d)\n",
             req->queue_msg, req->queue_tag, req->aux);
  YS__logmsg(nid, "  orig_request(%s), start_block(%d), current_length(%d)\n",
             SCSI_ReqName[req->orig_request], req->start_block, req->current_length);
  YS__logmsg(nid, "  transferred(%d), cache_segment(%d), current_data_size(%d)\n",
             req->transferred, req->cache_segment, req->current_data_size);
  YS__logmsg(nid, "  buscycles(%d)\n", req->buscycles);
       
}

void SCSI_bus_dump(SCSI_BUS *psbus)
{
  int nid = psbus->node_id;
  int n;

  YS__logmsg(nid, "\n============== SCSI BUS %d ==============\n",
	     psbus->bus_id);
  YS__logmsg(nid, "state(%d)\n", psbus->state);
  YS__logmsg(nid, "attached devices: ");
  for (n = 0; n < SCSI_WIDTH*8; n++)
    if (psbus->devices[n])
      YS__logmsg(nid, "%i ", n);

  YS__logmsg(nid, "\ncurrent_req\n");
  if (psbus->current_req)
    SCSI_req_dump(psbus->current_req, 0, nid);
  else
    YS__logmsg(nid, "  NULL\n");

  for (n = 0; n < SCSI_WIDTH*8; n++)
    if ((psbus->devices[n]) && (psbus->devices[n]->req))
      {
        YS__logmsg(nid, "device %i arbitrating\n", n);
        SCSI_req_dump(psbus->devices[n]->req, 0, nid);
      }

  YS__logmsg(nid, "arbitrate scheduled: %s\n",
             IsScheduled(psbus->arbitrate) ? "yes" : "no");
  YS__logmsg(nid, "send_request scheduled: %s\n",
             IsScheduled(psbus->send_request) ? "yes" : "no");
  YS__logmsg(nid, "send_response scheduled: %s\n",
             IsScheduled(psbus->send_response) ? "yes" : "no");

  for (n = 0; n < SCSI_WIDTH * 8; n++)
    if ((psbus->devices[n] != NULL) &&
	(psbus->devices[n]->dump != NULL))
      psbus->devices[n]->dump(psbus->devices[n]->device);
}


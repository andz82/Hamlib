/*
 *  Hamlib Kenwood backend - TH-G71 description
 *  Copyright (c) 2003-2004 by Stephane Fillod
 *
 *	$Id: thg71.c,v 1.13 2004-06-13 12:35:30 fillods Exp $
 *
 *   This library is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU Library General Public License as
 *   published by the Free Software Foundation; either version 2 of
 *   the License, or (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU Library General Public License for more details.
 *
 *   You should have received a copy of the GNU Library General Public
 *   License along with this library; if not, write to the Free Software
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <stdio.h>
#include <string.h>  /* String function definitions */
#include <unistd.h>  /* UNIX standard function definitions */

#include <hamlib/rig.h>
#include "kenwood.h"
#include "th.h"

#if 1
#define RIG_ASSERT(x)	if (!(x)) { rig_debug(RIG_DEBUG_ERR, "Assertion failed on line %i\n",__LINE__); abort(); }
#else
#define RIG_ASSERT(x)
#endif

#define THG71_FUNC_ALL (\
                       RIG_FUNC_TBURST \
                       )

#define THG71_LEVEL_ALL (\
			RIG_LEVEL_RAWSTR| \
                        RIG_LEVEL_SQL| \
                        RIG_LEVEL_RFPOWER\
			)

#ifndef RIG_TONEMAX
#define RIG_TONEMAX     38
#endif

#define RIG_VFO_A_OP (RIG_OP_UP|RIG_OP_DOWN)

#define ACKBUF_LEN 128

const struct kenwood_priv_caps  thg71_priv_caps  = {
    .cmdtrm =  EOM_TH,   /* Command termination character */
};
#define EOM EOM_TH


/* thg71 procs */
int thg71_open(RIG *rig);
int thg71_decode_event (RIG *rig);
int thg71_set_freq (RIG *rig, vfo_t vfo, freq_t freq);
int thg71_get_mode (RIG *rig, vfo_t vfo, rmode_t *mode, pbwidth_t *width);
int thg71_set_vfo (RIG *rig, vfo_t vfo);
int thg71_get_vfo (RIG *rig, vfo_t *vfo);
int thg71_set_func(RIG *rig, vfo_t vfo, setting_t func, int status);

/*
 * th-g71 rig capabilities.
 */
const struct rig_caps thg71_caps = {
.rig_model =  RIG_MODEL_THG71,
.model_name = "TH-G71",
.mfg_name =  "Kenwood",
.version =  "0.1",
.copyright =  "LGPL",
.status =  RIG_STATUS_BETA,
.rig_type =  RIG_TYPE_HANDHELD,
.ptt_type =  RIG_PTT_RIG,
.dcd_type =  RIG_DCD_RIG,
.port_type =  RIG_PORT_SERIAL,
.serial_rate_min =  9600,
.serial_rate_max =  9600,
.serial_data_bits =  8,
.serial_stop_bits =  1,
.serial_parity =  RIG_PARITY_NONE,
.serial_handshake =  RIG_HANDSHAKE_NONE,
.write_delay =  1,
.post_write_delay =  0,
.timeout =  500,
.retry =  3,

.has_set_func =  THG71_FUNC_ALL,
.has_get_level =  THG71_LEVEL_ALL,
.has_set_level =  RIG_LEVEL_SET(THG71_LEVEL_ALL),
.level_gran = {
	[LVL_RAWSTR] = { .min = { .i = 0 }, .max = { .i = 5 } },
	[LVL_SQL] = { .min = { .i = 0 }, .max = { .i = 5 } },
	[LVL_RFPOWER] = { .min = { .i = 3 }, .max = { .i = 0 } },
},
.parm_gran =  {},
.ctcss_list =  kenwood38_ctcss_list,
.dcs_list =  NULL,
.preamp =   { RIG_DBLST_END, },
.attenuator =   { RIG_DBLST_END, },
.max_rit =  Hz(0),
.max_xit =  Hz(0),
.max_ifshift =  Hz(0),
.vfo_ops =  RIG_VFO_A_OP,
.targetable_vfo =  RIG_TARGETABLE_NONE,
.transceive =  RIG_TRN_RIG,
.bank_qty =   0,
.chan_desc_sz =  6,


.chan_list =  { 
		{  1,  199, RIG_MTYPE_MEM , {TH_CHANNEL_CAPS}},  /* normal MEM */
		{  200,219, RIG_MTYPE_EDGE , {TH_CHANNEL_CAPS}}, /* U/L MEM */
		{  220,220, RIG_MTYPE_MEM , {TH_CHANNEL_CAPS}},  /* Priority */
		{  221,222, RIG_MTYPE_MEM , {TH_CHANNEL_CAPS}},  /* Call 0/1 */
		{  223,231, RIG_MTYPE_MEM , {TH_CHANNEL_CAPS}},  /* Band VFO */
			 RIG_CHAN_END,
		   },

/* no rx/tx_range_list */
/* computed in thg71_open */

.tuning_steps =  {
	 {RIG_MODE_FM,kHz(5)},
	 {RIG_MODE_FM,kHz(6.25)},
	 {RIG_MODE_FM,kHz(10)},
	 {RIG_MODE_FM,kHz(12.5)},
	 {RIG_MODE_FM,kHz(15)},
	 {RIG_MODE_FM,kHz(20)},
	 {RIG_MODE_FM,kHz(25)},
	 {RIG_MODE_FM,kHz(30)},
	 {RIG_MODE_FM,kHz(50)},
	 {RIG_MODE_FM,kHz(100)},
	 RIG_TS_END,
	},
        /* mode/filter list, remember: order matters! */
.filters =  {
	{RIG_MODE_FM, kHz(12)},
	{RIG_MODE_AM, kHz(9)},
	RIG_FLT_END,
	},

.str_cal ={ 2, { {0, -30 }, {5,-13}}}, /* guessed from technical manual */

.priv =  (void *)&thg71_priv_caps,
.rig_open =  thg71_open,
.rig_close =  NULL,

.set_freq =  th_set_freq,
.get_freq =  th_get_freq,
.get_mode =  thg71_get_mode,
.set_vfo =  thg71_set_vfo,
.get_vfo =  thg71_get_vfo,
.set_mem =  th_set_mem,
.get_mem =  th_get_mem,
.set_channel =  th_set_channel,
.get_channel =  th_get_channel,
.set_trn =  th_set_trn,
.get_trn =  th_get_trn,

.set_func =  thg71_set_func,
.get_level = th_get_level,
.set_level = th_set_level,
.get_info =  th_get_info,
.vfo_op = th_vfo_op,
.set_ptt = th_set_ptt,
.get_dcd=th_get_dcd,
.decode_event =  thg71_decode_event,
};


/* --------------------------------------------------------------------- */
int thg71_decode_event (RIG *rig)
{
    char asyncbuf[ACKBUF_LEN];
    int retval, asyncbuf_len = ACKBUF_LEN-1;
    rig_debug(RIG_DEBUG_TRACE, "%s: called\n", __FUNCTION__);

    retval = kenwood_transaction(rig, NULL, 0, asyncbuf, &asyncbuf_len);
    if (retval != RIG_OK)
        return retval;

    rig_debug(RIG_DEBUG_TRACE, "%s: Decoding message\n", __FUNCTION__);

    if (asyncbuf[0] == 'B' && asyncbuf[1] == 'U' && asyncbuf[2] == 'F') {

        freq_t freq, offset;
        int step, shift, rev, tone, ctcss, tonefq, ctcssfq;

        retval = sscanf(asyncbuf, "BUF 0,%"FREQFMT",%d,%d,%d,%d,%d,,%d,,%d,%"FREQFMT,
                                  &freq, &step, &shift, &rev, &tone,
                                  &ctcss, &tonefq, &ctcssfq, &offset);
        if (retval != 11) {
            rig_debug(RIG_DEBUG_ERR, "%s: Unexpected BUF message '%s'\n", __FUNCTION__, asyncbuf);
            return -RIG_ERJCTED;
        }

        rig_debug(RIG_DEBUG_TRACE, "%s: Buffer (freq %"FREQFMT" Hz, mode %d)\n", __FUNCTION__, freq);

        /* Callback execution */
        if (rig->callbacks.vfo_event) {
            rig->callbacks.vfo_event(rig, RIG_VFO_A, rig->callbacks.vfo_arg);
        }
        if (rig->callbacks.freq_event) {
            rig->callbacks.freq_event(rig, RIG_VFO_A, freq, rig->callbacks.freq_arg);
        }
	/* 
        if (rig->callbacks.mode_event) {
            rig->callbacks.mode_event(rig, RIG_VFO_A, mode, RIG_PASSBAND_NORMAL,
							rig->callbacks.mode_arg);
        }
	*/

    /* --------------------------------------------------------------------- */
    } else if (asyncbuf[0] == 'S' && asyncbuf[1] == 'M') {

        int lev;
        retval = sscanf(asyncbuf, "SM 0,%d", &lev);
        if (retval != 2) {
            rig_debug(RIG_DEBUG_ERR, "%s: Unexpected SM message '%s'\n", __FUNCTION__, asyncbuf);
            return -RIG_ERJCTED;
        }

        rig_debug(RIG_DEBUG_TRACE, "%s: Signal strength event - signal = %.3f\n", __FUNCTION__, (float)(lev / 5.0));

        /* Callback execution */
#if STILLHAVETOADDCALLBACK
        if (rig->callbacks.strength_event)
            rig->callbacks.strength_event(rig, RIG_VFO_0,(float)(lev / 5.0),
							rig->callbacks.strength_arg);
#endif

    /* --------------------------------------------------------------------- */
    } else if (asyncbuf[0] == 'B' && asyncbuf[1] == 'Y') {

        int busy;

        retval = sscanf(asyncbuf, "BY 0,%d", &busy);
        if (retval != 2) {
            rig_debug(RIG_DEBUG_ERR, "%s: Unexpected BY message '%s'\n", __FUNCTION__, asyncbuf);
            return -RIG_ERJCTED;
        }
        rig_debug(RIG_DEBUG_TRACE, "%s: Busy event - status = '%s'\n",
				__FUNCTION__, (busy == 0) ? "OFF" : "ON" );
        return -RIG_ENIMPL;
        /* This event does not have a callback. */

    /* --------------------------------------------------------------------- */
    } else if (asyncbuf[0] == 'V' && asyncbuf[1] == 'M' && asyncbuf[2] == 'C') {

        vfo_t bandmode;

        retval = sscanf(asyncbuf, "VMC 0,%d", &bandmode);
        if (retval != 1) {
            rig_debug(RIG_DEBUG_ERR, "%s: Unexpected VMC message '%s'\n", __FUNCTION__, asyncbuf);
            return -RIG_ERJCTED;
        }

        switch (bandmode) {
            case 0:     bandmode = RIG_VFO_VFO;  break;
            case 2:     bandmode = RIG_VFO_MEM;  break;
          /*  case 3:     bandmode = RIG_VFO_CALL; break; */
            default:    bandmode = RIG_VFO_CURR; break; 
        }
        rig_debug(RIG_DEBUG_TRACE, "%s: Mode of Band event -  %d\n", __FUNCTION__,  bandmode);

        /* TODO: This event does not have a callback! */
        return -RIG_ENIMPL;
    /* --------------------------------------------------------------------- */
    } else {

        rig_debug(RIG_DEBUG_ERR, "%s: Unsupported transceive cmd '%s'\n", __FUNCTION__, asyncbuf);
        return -RIG_ENIMPL;
    }

    return RIG_OK;
}

/* --------------------------------------------------------------------- */
int thg71_get_mode (RIG *rig, vfo_t vfo, rmode_t *mode, pbwidth_t *width)
{
    char ackbuf[ACKBUF_LEN];
    int retval,ack_len=ACKBUF_LEN;
    int step;
    freq_t freq;

    rig_debug(RIG_DEBUG_TRACE, "%s: called\n", __FUNCTION__);

	switch (vfo) {
      case RIG_VFO_CURR: break;
      case RIG_VFO_A: break;
	  default:
        rig_debug(RIG_DEBUG_ERR, "%s: Unsupported VFO %d\n", __FUNCTION__, vfo);
		return -RIG_EVFO;
	}

	/* try to guess from frequency */
    retval = kenwood_transaction(rig, "FQ"EOM, 3, ackbuf, &ack_len);
        if (retval != RIG_OK)
        return retval;

    sscanf(ackbuf,"FQ %"FREQFMT",%d",&freq,&step);

    if(freq <MHz(136) )  {
		*mode=RIG_MODE_AM;
		*width=kHz(9);
	} else {
		*mode=RIG_MODE_FM;
		*width=kHz(12);
	}

	return RIG_OK;
}

/* --------------------------------------------------------------------- */
int thg71_set_vfo (RIG *rig, vfo_t vfo)
{
    char vfobuf[16], ackbuf[ACKBUF_LEN];
    int retval,ack_len=ACKBUF_LEN;

    rig_debug(RIG_DEBUG_TRACE, "%s: called\n", __FUNCTION__);

        switch (vfo) {
        case RIG_VFO_A:
        case RIG_VFO_VFO:
            sprintf(vfobuf, "VMC 0,0"EOM);
            break;
        case RIG_VFO_MEM:
            sprintf(vfobuf, "VMC 0,2"EOM);
            break;
        default:
            rig_debug(RIG_DEBUG_ERR, "%s: Unsupported VFO %d\n", __FUNCTION__, vfo);
            return -RIG_EVFO;
        }

    retval = kenwood_transaction(rig, vfobuf, strlen(vfobuf), ackbuf, &ack_len);
        if (retval != RIG_OK)
        return retval;

    return RIG_OK;
}

/* --------------------------------------------------------------------- */
int thg71_get_vfo (RIG *rig, vfo_t *vfo)
{
    char ackbuf[ACKBUF_LEN];
    int retval,ack_len=ACKBUF_LEN;
    int vch;

    retval = kenwood_transaction(rig, "VMC 0"EOM , 6, ackbuf, &ack_len);
        if (retval != RIG_OK)
        return retval;

    sscanf(ackbuf,"VMC 0,%d"EOM,&vch);
    switch(vch) {
	case 0:
		*vfo=RIG_VFO_A;
		break;
	case 1:
	case 2:
		*vfo=RIG_VFO_MEM;
		break;
        default:
            rig_debug(RIG_DEBUG_ERR, "%s: Unsupported VFO %d\n", __FUNCTION__, vfo);
            return -RIG_EVFO;
	}
	return RIG_OK;
}

/* --------------------------------------------------------------------- */
int thg71_set_func(RIG *rig, vfo_t vfo, setting_t func, int status)
{
    char ackbuf[ACKBUF_LEN];
    int retval,ack_len=ACKBUF_LEN;

     if(func != RIG_FUNC_TBURST)
		return -RIG_EINVAL;

    if(status==1) {
    retval = kenwood_transaction(rig, "TT"EOM, 3, ackbuf, &ack_len);
        if (retval != RIG_OK)
        	return retval;

	return RIG_OK;
    }
    if(status==0) {
	return rig_set_ptt(rig,vfo,RIG_PTT_OFF);
    }
    return -RIG_EINVAL;
}

/* --------------------------------------------------------------------- */
int thg71_open(RIG *rig)
{
    char ackbuf[ACKBUF_LEN],*strl,*stru;
    int retval,i,ack_len=ACKBUF_LEN;
    const freq_range_t frend=RIG_FRNG_END;

	/* just to be sure it's a THG-71 */
    retval = kenwood_transaction(rig, "ID"EOM, 3, ackbuf, &ack_len);
        if (retval != RIG_OK)
        	return retval;
    
    if (ack_len<9 || strncmp(ackbuf,"ID TH-G71",9)) {
        rig_debug(RIG_DEBUG_ERR, "%s: Unexpected reply '%s'\n", __FUNCTION__, ackbuf);
        return -RIG_ERJCTED;
    }

	/* fill state.rx/tx range_list */
    ack_len=ACKBUF_LEN;
    retval = kenwood_transaction(rig, "FL"EOM, 3, ackbuf, &ack_len);
        if (retval != RIG_OK)
        	return retval;

    strl=strtok(ackbuf," ");
    for(i=0;i<FRQRANGESIZ;i++) {
		freq_range_t frng;

	    	strl=strtok(NULL,",");
	    	stru=strtok(NULL,",");
		if(strl==NULL && stru==NULL)
			break;
		frng.start=MHz(atoi(strl));
		frng.end=MHz(atoi(stru));
		frng.vfo=RIG_VFO_A;
		frng.ant=0;
		if(frng.end<=MHz(135))
			frng.modes=RIG_MODE_AM;
		else
			frng.modes=RIG_MODE_FM;

		frng.high_power=-1;
		frng.low_power=-1;
		rig->state.rx_range_list[i]=frng;
		if(frng.start> MHz(200))
			frng.high_power=5.5;
		else
			frng.high_power=6;
		frng.low_power=mW(50);
		rig->state.tx_range_list[i]=frng;
	}
	rig->state.rx_range_list[i]= frend;
	rig->state.tx_range_list[i]= frend;

    return RIG_OK;
}


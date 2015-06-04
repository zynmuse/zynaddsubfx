/*
  ZynAddSubFX - a software synthesizer

  minimal.cpp - Audio functions for the minimal sequencer
  Copyright (C) 2002 Nasca Octavian Paul
  Author: Nasca Octavian Paul

  This program is free software; you can redistribute it and/or modify
  it under the terms of version 2 of the GNU General Public License
  as published by the Free Software Foundation.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License (version 2 or later) for more details.

  You should have received a copy of the GNU General Public License (version 2)
  along with this program; if not, write to the Free Software Foundation,
  Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA

*/
// TODO: GPL 3

#include "../globals.h"
#include "../Misc/Util.h"
#include "../Misc/MiddleWare.h"
#include "../Misc/Master.h"

#include "minimal.h"

//! global variables required for linking:
const char *instance_name = 0;
SYNTH_T  *synth;
MiddleWare *middleware;
#if USE_NSM
#include "../UI/NSM.H"

NSM_Client *nsm = 0;
#endif

#include <thread>
#include <unistd.h>
#include <climits>

class ZynMinimalPlugin : public minimal_plugin
{
	// globals are annoying. we can not use them in this class
	//MiddleWare *middleware;
	//SYNTH_T *synth;
	sample_rate_t sample_rate;
	std::thread* loadThread;
	using snd_seq_event_t = int;
	void run_synth(unsigned long sample_count, snd_seq_event_t *, unsigned long);
	
	float *outl;
	float *outr;
public:
	ZynMinimalPlugin(sample_rate_t srate) : sample_rate(srate) {}
	void _send_osc_cmd(const char* cmd) {
		middleware->transmitMsg(cmd);
	}
	void prepare();
	bool proceed(sample_t sample_count) {
		run_synth(sample_count, nullptr, 0ul);
		return true;
	}
};

void ZynMinimalPlugin::prepare()
{
	synth = new SYNTH_T;
	synth->samplerate = sample_rate;
	
	//    this->sampleRate  = srate;
	//    this->banksInited = false;
	
	config.init();
	
	sprng(time(NULL));
	denormalkillbuf = new float [synth->buffersize];
	for(int i = 0; i < synth->buffersize; i++)
	denormalkillbuf[i] = (RND - 0.5f) * 1e-16;
	
	synth->alias();
	middleware = new MiddleWare();
	middleware->spawnMaster()->bank.rescanforbanks();
	loadThread = new std::thread([this]() {
		while(middleware) {
		middleware->tick();
		usleep(1000);
		}});
}

void ZynMinimalPlugin::run_synth(unsigned long sample_count,
		snd_seq_event_t *,
		unsigned long )
{
    unsigned long from_frame       = 0;
//  unsigned long event_index      = 0;
    unsigned long next_event_frame = 0;
    unsigned long to_frame = 0;

    Master *master = middleware->spawnMaster();

    do {
        /* Find the time of the next event, if any */
#if 0
        if((events == NULL) || (event_index >= event_count))
            next_event_frame = ULONG_MAX;
        else
            next_event_frame = events[event_index].time.tick;
#else
        next_event_frame = ULONG_MAX;
#endif
        /* find the end of the sub-sample to be processed this time round... */
        /* if the next event falls within the desired sample interval... */
        if((next_event_frame < sample_count) && (next_event_frame >= to_frame))
            /* set the end to be at that event */
            to_frame = next_event_frame;
        else
            /* ...else go for the whole remaining sample */
            to_frame = sample_count;
        if(from_frame < to_frame) {
            // call master to fill from `from_frame` to `to_frame`:
            master->GetAudioOutSamples(to_frame - from_frame,
                                       (int)sample_rate,
                                       &(outl[from_frame]),
                                       &(outr[from_frame]));
            // next sub-sample please...
            from_frame = to_frame;
        }
#if 0
        // Now process any event(s) at the current timing point
        while(events != NULL && event_index < event_count
              && events[event_index].time.tick == to_frame) {
            if(events[event_index].type == SND_SEQ_EVENT_NOTEON)
                master->noteOn(events[event_index].data.note.channel,
                               events[event_index].data.note.note,
                               events[event_index].data.note.velocity);
            else
            if(events[event_index].type == SND_SEQ_EVENT_NOTEOFF)
                master->noteOff(events[event_index].data.note.channel,
                                events[event_index].data.note.note);
            else
            if(events[event_index].type == SND_SEQ_EVENT_CONTROLLER)
                master->setController(events[event_index].data.control.channel,
                                      events[event_index].data.control.param,
                                      events[event_index].data.control.value);
            else {}
            event_index++;
        }
#endif

        // Keep going until we have the desired total length of sample...
    } while(to_frame < sample_count);
}

minimal_plugin *instantiate(unsigned long sample_rate)
{
	return new ZynMinimalPlugin(sample_rate);
	//return const_cast<char*>("abcdefghhhhhhhhhhhhhhhhhhhhh");
}


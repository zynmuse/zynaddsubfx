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

namespace mini {

zyn_tree_t::zyn_tree_t(const char *name) :
	nnode("", nullptr),
	audio_instrument_t(name),
	notes_t_port(this)
{
}

command_base *zynaddsubfx_t::make_close_command() const
{
	return new command<>("/close-ui");
}

void zynaddsubfx_t::prepare()
{
set_sample_rate(44100); // TODO!!


    std::clog << "Starting zynaddsubfx..." << std::endl;
    std::clog << "  (this is a good sign...)" << std::endl;
    
	synth = new SYNTH_T;
	//synth->samplerate = sample_rate; // TODO!!!
	
	//    this->sampleRate  = srate;
	//    this->banksInited = false;
	
	config.init();
	
	sprng(::time(nullptr));
	denormalkillbuf = new float [synth->buffersize];
	for(int i = 0; i < synth->buffersize; i++)
	 denormalkillbuf[i] = (RND - 0.5f) * 1e-16;
	
	synth->alias();
	middleware = new MiddleWare();
	middleware->spawnMaster()->bank.rescanforbanks();
/*	loadThread = new std::thread([this]() {
		while(middleware) {
		middleware->tick();
		usleep(1000);
		}});*/
}

struct master_functor
{
    Master* _master;
    sample_rate_t sample_rate;
    void operator()(std::size_t , std::size_t amnt,
        mini::Stereo<float>* dest)
    {
        io::mlog << "amnt: " << amnt << io::endl;
        // first arg is not needed: master currently counts it itself
        // (TODO: allow master to start at offset)
        if(! dest)
         io::mlog << "WARNING 1..." << io::endl;
        io::mlog << "buf2: " << dest << io::endl;
        _master->GetAudioOutSamplesStereo(amnt, (int)sample_rate, dest);
    }
};

void zynaddsubfx_t::run_synth(unsigned long ,
		snd_seq_event_t *,
		unsigned long )
{
    if(samples_played <= time())
    {
    Master *master = middleware->spawnMaster();

//    io::mlog << "write space: " << data.write_space()
//        << " samples: " << sample_count << io::endl;
    
    if( data.write_space() < (std::size_t)synth->buffersize )
        throw "warning: not enough write space! :-(";
    
    io::mlog << "buf1: " << &data << io::endl;

    master_functor mf { master, zynaddsubfx_t::sample_rate };
    data.write_func(mf, synth->buffersize); // TODO: not sure about this value
    samples_played += synth->buffersize;
    }
    
    
     // TODO: not sure about this value:
    set_next_time(time() + synth->buffersize);
    
    
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
}

void zynaddsubfx_t::send_osc_cmd(const char * msg)
{
    middleware->transmitMsg(msg);
}

/*mini::instrument_t *instantiate(unsigned long sample_rate)
{
	return new zynaddsubfx_t(sample_rate);
	//return const_cast<char*>("abcdefghhhhhhhhhhhhhhhhhhhhh");
}*/

void zynaddsubfx_t::set_sample_rate(sample_rate_t srate) { zynaddsubfx_t::sample_rate = srate; }

bool zynaddsubfx_t::advance() {
	//assert(sample_rate);
	run_synth(0, nullptr, 0ul);
	return true;
}

zyn_tree_t::notes_t_port_t::notes_t_port_t(zyn_tree_t *parent) :
	// todo: base, ext does not make sense here?
	rtosc_in_port_t<notes_in>(*parent->get_ins()),
	parent_ptr(parent),
	ins(parent->get_ins())
{
	note_ons.reserve(NOTES_MAX);
	note_offs.reserve(NOTES_MAX);
	for(std::size_t idx = 0; idx < NOTES_MAX; ++idx)
	{
		note_ons.emplace_back(parent, ins, 0 /*chan*/, idx/*offs*/,
			self_port_templ<int, true>{});
		note_offs.emplace_back(parent, ins, 0 /*chan*/, idx/*offs*/,
			self_port_templ<int, true>{});
	}

	set_trigger(); // TODO: here?
}

void zyn_tree_t::notes_t_port_t::on_read(sample_no_t pos)
{
	io::mlog << "zyn notes port::on_read" << io::endl;
	for(const std::pair<int, int>& rch : notes_in::data->recently_changed)
	if(rch.first < 0)
	 break;
	else
	{
		// for self_port_t, on_read is not virtual, so we call it manually...
		// -> TODO?? probably the above comment is deprecated
		std::pair<int, int> p2 = notes_in::data->lines[rch.first][rch.second];

		io::mlog << "first, second: " << p2.first << ", " << p2.second << io::endl;

		if(p2.first >= 0) // i.e. note on
		{
			m_note_on_t& note_on_cmd = note_ons[rch.first];
			// self_port_t must be completed manually:
			note_on_cmd.cmd_ptr->port_at<2>().set(p2.second);
			note_on_cmd.cmd_ptr->command::update();

			note_on_cmd.cmd_ptr->complete_buffer(); // TODO: call in on_read??
		}

		auto& cmd_ref = (p2.first < 0)
			? note_offs[rch.first].cmd
			: note_ons[rch.first].cmd;

		// TODO!!
		// note_offs[p.first].on_read();
		if(cmd_ref.set_changed())
		{
			cmd_ref.update_next_time(pos); // TODO: call on recv
			ins->update(cmd_ref.get_handle());
		}
	}
}

}


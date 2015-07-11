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

#ifndef MINIMAL_H
#define MINIMAL_H

class MiddleWare;
struct SYNTH_T;

// TODO: include own header
// #include <iostream> // TODO!!
#include <map>

#include <vector>
#include "minimal/sample.h"
#include "minimal/audio_instrument.h" // TODO: separate audio_instrument_t ?
#include "minimal/command_tools.h"
#include "minimal/mports.h"
//#include "impl.h" // currently unused
#include "minimal/io.h"

#include "minimal_external.h"

namespace std
{
	class thread;
}

namespace mini
{

class zyn_tree_t;

namespace zyn {

template<class PortT>
using port = in_port_with_command<zyn_tree_t, PortT>;

}

template<class Access>
class single_ptr
{
	Access* acc = nullptr;
public:
	template<class ...Args>
	Access* get(Args... args) {
		return (acc == nullptr)
			? (acc = new Access(args...))
			: acc;
	}
	Access* operator->() {
		return get<>();
	}
};

class nnode : public named_t
{
private:
	void register_as_child(nnode* child) {
		children.emplace(child, false);
	}
protected: // TODO?
	// TODO: call this the other way in the future?
	virtual instrument_t* get_ins() {
		return parent->get_ins();
	}

protected:
	std::map<nnode*, bool> children;
	nnode* parent = nullptr;
public:
	struct ctor
	{
		class nnode* parent;
		const char* name;
		ctor(nnode* parent, const char* name) :
			parent(parent),
			name(name) {}
	};

	nnode(const char* ext, nnode* parent) :
		named_t(ext), parent(parent) {
		if(parent)
		 parent->register_as_child(this);
	}

	void print_parent_chain() {
		no_rt::mlog << name() << " -> " << std::endl;
		if(parent)
		 parent->print_parent_chain();
		else
		 no_rt::mlog << "(root)" << std::endl;
	}

	void print_tree(unsigned initial_depth = 0) {
		for(unsigned i = 0; i < initial_depth; ++i)
		 no_rt::mlog << "  ";

		no_rt::mlog << "- " << name() << std::endl;
		unsigned next_depth = initial_depth + 1;
		for(const auto& pr : children)
		 pr.first->print_tree(next_depth) ;
	}


	std::string full_path() const {
		return parent
			? parent->full_path() + "/" + name()
			: /*"/" +*/ name(); // TODO: why does this work??
	}

	template<class PortT> PortT& add_if_new(const std::string& ext)
	{
		return *new PortT(get_ins(), full_path() + "/", ext);
		//return static_cast<PortT&>(
		//	*used_ch.emplace(ext, new NodeT(ins, name(), ext)).
		//	first->second);
	}

	template<class PortT>
	PortT& spawn(const std::string& ext) {
		return add_if_new<PortT>(ext);
	}
};

template<class NodeT, char... Ext>
class build : single_ptr<NodeT>
{
protected:
	nnode* parent = nullptr;
	const char* name = "/";
public:
	//nnode_builder(nnode* parent) : parent(parent) {}
	build() = default;

	build(const nnode::ctor& ctor) :
		parent(ctor.parent),
		name(ctor.name)
		{}

	NodeT* operator->() {
		return single_ptr<NodeT>::get(name, parent);
	}
};

struct kit0 : nnode
{
	using nnode::nnode;
};

class amp_env_t : public nnode
{
public:
	using nnode::nnode;
	template<class Port>
	zyn::port<Port>& envsustain() {
		return spawn<zyn::port<Port>>("Penvsustain");
		//return new param_t("envsustain", ins);

	} // TODO: own envsustain class with operator() ?

	/*template<class Port1>
	zyn::p_envsustain<Port1> envsustain(oint<Port1> con) const {
		return p_envsustain<Port1>(con);
	}*/
};

class global : public nnode
{
public:
	using nnode::nnode;
	/*amp_env_t amp_env() {
		return spawn<amp_env_t>("AmpEnvelope");
	}*/
	build<amp_env_t> amp_env_t = ctor(this, "AmpEnvelope");
};


class voice0 : public nnode
{
public:
	using nnode::nnode;
};

class padpars : public nnode
{
	void on_preinit() { // TODO
//		ins->add_const_command(command<bool>("... pad enabled", true));
	}
public:
	using nnode::nnode;
};

class adpars : public nnode
{
	void on_preinit() { // TODO
//		ins->add_const_command(command<bool>("... add enabled", true));
	}
public:
	using nnode::nnode;
	build<voice0> voice0 = ctor(this, "voice0");
	build<global> global = ctor(this, "global");

};


template<class = void, bool = false>
class use_no_port {};

template<template<class, bool> class P, class T>
struct _port_type_of { using type = P<T, true>; };

template<class T>
struct _port_type_of<use_no_port, T> { using type = T; };

template<template<class , bool> class P, class T>
using port_type_of = typename _port_type_of<P, T>::type;

class zyn_tree_t : public nnode, public audio_instrument_t
{
	// todo: only send some params on new note?
public:
	template<template<class , bool> class Port1 = use_no_port,
		template<class , bool> class Port2 = use_no_port,
		template<class , bool> class Port3 = use_no_port>
	class note_on : public in_port_with_command<zyn_tree_t, port_type_of<Port1, int>, port_type_of<Port2, int>, port_type_of<Port3, int>>
	{
		using base = in_port_with_command<zyn_tree_t, port_type_of<Port1, int>, port_type_of<Port2, int>, port_type_of<Port3, int>>;
	public:
		note_on(zyn_tree_t* zyn, port_type_of<Port1, int> chan, port_type_of<Port2, int> note, port_type_of<Port3, int>&& velocity) // TODO: rvals
			: base(zyn, "/", "noteOn", chan, note, std::forward<port_type_of<Port3, int>>(velocity)) // TODO: forward instead of move?
		{
		}
	};

	template<template<class , bool> class Port1 = use_no_port,
		template<class , bool> class Port2 = use_no_port,
		template<class , bool> class Port3 = use_no_port>
	class note_off : public in_port_with_command<zyn_tree_t, port_type_of<Port1, int>, port_type_of<Port2, int>, port_type_of<Port3, int>>
	{
		using base = in_port_with_command<zyn_tree_t, port_type_of<Port1, int>, port_type_of<Port2, int>, port_type_of<Port3, int>>;
	public:
		note_off(zyn_tree_t* zyn, port_type_of<Port1, int> chan, port_type_of<Port2, int> note, port_type_of<Port3, int>&& id)
			: base(zyn, "/", "noteOff", chan, note, std::forward<port_type_of<Port3, int>>(id))
		{
		}
	};
private:
	using c_note_on = note_on<use_no_port, use_no_port, self_port_templ>;
	using c_note_off = note_off<use_no_port, use_no_port, self_port_templ>;

	template<class InstClass>
	struct notes_t_port_t : node_t<InstClass>, rtosc_in_port_t<notes_in>
	{
		command_base* cmd;
		InstClass* ins;
		using m_note_on_t = note_on<use_no_port, use_no_port, self_port_templ>;
		using m_note_off_t = note_off<use_no_port, use_no_port, self_port_templ>;

		std::vector<m_note_on_t> note_ons;
		std::vector<m_note_off_t> note_offs;
	public:
		notes_t_port_t(InstClass* ins, const std::string& base, const std::string& ext) : // todo: base, ext does not make sense here?
			node_t<InstClass>(ins, base, ext),
			rtosc_in_port_t<notes_in>(*ins),
			ins(ins)
		{
			note_ons.reserve(NOTES_MAX);
			note_offs.reserve(NOTES_MAX);
			for(std::size_t idx = 0; idx < NOTES_MAX; ++idx)
			{
				note_ons.emplace_back(ins, 0 /*chan*/, idx/*offs*/, self_port_templ<int, true>{});
				note_offs.emplace_back(ins, 0 /*chan*/, idx/*offs*/, self_port_templ<int, true>{});
			}

			set_trigger(); // TODO: here?
		}

		void on_read(sample_no_t pos)
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
	};

	notes_t_port_t<zyn_tree_t> notes_t_port; // TODO: inherit??


public:

	zyn_tree_t(const char* name);
	virtual ~zyn_tree_t() {}

	/*
	 *  ports
	 */

	//build<add0> add0; //= ctor(this, "voice0");


	/*zyn::adpars add0() {
		//return spawn<zyn::adpars>("part0/kit0/adpars");
		return part0().kit0().adpars();
	}

	zyn::padpars pad0() {
		return spawn<zyn::padpars>("part0/kit0/padpars");
	}*/
	// TODO: add0;

	instrument_t* get_ins() { return this; }
	class adpars : public nnode
	{
		void on_preinit() { // TODO
	//		ins->add_const_command(command<bool>("... add enabled", true));
		}
	public:
		using nnode::nnode;
		build<voice0> voice0 = ctor(this, "voice0");
		build<global> global = ctor(this, "global");

	};

	notes_t_port_t<zyn_tree_t>& note_input() {
		return notes_t_port;
	}


	struct fx_t : public nnode
	{
		template<class Port>
		zyn::port<Port>& efftype() { // TODO: panning must be int...
			return spawn<zyn::port<Port>>("efftype");
		}
		// TODO: for debugging features, efftype should be a node, too

		// TODO: template is useless
		template<class Port>
		zyn::port<Port>& eff0_part_id() { // TODO: panning must be int...
			return *new zyn::port<Port>(get_ins(), "/", "Pinsparts0");
			//return spawn_new<zyn::port<Port>>("efftype");
		}
		using nnode::nnode;
		//build<voice0> voice0 = ctor(this, "voice0");
		//build<global> global = ctor(this, "global");
	};

	class kit_t : public nnode
	{
	public:
		using nnode::nnode;
		build<adpars> adpars = ctor(this, "adpars");
	};

	class part_t : public nnode
	{
	public:
		using nnode::nnode;
		//zyn::amp_env amp_env() const {
		//	return spawn<zyn::amp_env>("AmpEnvelope/");
		//}
		template<class Port>
		zyn::port<Port>& Ppanning() { // TODO: panning must be int...
			return spawn<zyn::port<Port>>("Ppanning");
		}

		/*template<std::size_t Id = 0>
		fx_t partefx() { // TODO: panning must be int...
			return spawn<fx_t, Id>("partefx");
		}

		kit_t kit0() {
			return spawn<kit_t>("kit0");
		}*/
		build<fx_t> partefx = ctor(this, "partefx0");
		build<kit0> kit0 = ctor(this, "kit0");

	//	using T = fx_t (zyn_tree_t::*)(const std::string& ext) const;
	//	const T partefx2 = &zyn_tree_t::spawn<fx_t, 0>;


		//using partefx2 = zyn_tree_t::spawn<fx_t, 0>; // TODO: instead, return struct which has operator()?
	};


	// TODO
	//template<std::size_t Id>
	//fx_t insefx0() { return spawn<fx_t, Id>("insefx"); }


	//template<std::size_t Id>
	//part_t part() { return spawn<part_t, Id>("part"); } // TODO: large tuple for these
	//part_t part0() { return part<0>(); }

	build<part_t> part0 = ctor(this, "part0");
	build<fx_t> insefx0 = ctor(this, "insefx0");


/*	// TODO???
	using volume_ptr_t = zyn::port<int>*(*)();
	//spawn_new<zyn::port<Port>>;
	volume_ptr_t volume = &spawn_new<zyn::port<int>>;
*/
	template<class Port>
	zyn::port<Port>& volume() {
		return spawn<zyn::port<Port>>("volume");
	}
};

class zynaddsubfx_t : public zyn_tree_t
{
	sample_no_t samples_played = 0;

	// globals are annoying. we can not use them in this class
	//MiddleWare *middleware;
	//SYNTH_T *synth;
	
	std::string make_start_command() const { return "/TODO"; }
	command_base* make_close_command() const;
	
	sample_rate_t sample_rate;
	std::thread* loadThread;
	
	using snd_seq_event_t = int;
	void run_synth(unsigned long, snd_seq_event_t *, unsigned long);
	
	float *outl; // TODO: -> into class?
	float *outr;
	
	std::size_t smp_pos = 0;
	
public:
	using zyn_tree_t::zyn_tree_t;
	~zynaddsubfx_t() = default;
	
	void send_osc_cmd(const char*msg);
	
	void set_sample_rate(sample_rate_t srate); // TODO: inline?
	void prepare();
	bool advance(); // TODO: inline?
	
	void instantiate_first() { prepare(); }
};

}

#endif // MINIMAL_H

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
class SYNTH_T;

// TODO: include own header
// #include <iostream> // TODO!!

#include <vector>
#include "minimal/audio_instrument.h" // TODO: separate audio_instrument_t ?
#include "minimal/command_tools.h"
#include "minimal/mports.h"
//#include "impl.h" // currently unused
#include "minimal/io.h"

#ifdef __cplusplus
extern "C" {
#endif

// TODO: not sure whether we need to return void*,
// however, this seems to work
minimal_plugin *instantiate(unsigned long sample_rate);

#ifdef __cplusplus
}
#endif

#endif // MINIMAL_H

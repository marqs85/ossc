//
// Copyright (C) 2015-2016  Markus Hiienkari <mhiienka@niksula.hut.fi>
//
// This file is part of Open Source Scan Converter project.
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//

#include "string.h"
#include "cfg.h"
#include "av_controller.h"


// Target configuration
extern avconfig_t tc;

// Current mode
extern avmode_t cm;

void set_default_avconfig()
{
    memset(&cm.cc, 0, sizeof(avconfig_t));
    memset(&tc, 0, sizeof(avconfig_t));
    cm.cc.pre_coast = DEFAULT_PRE_COAST;
    tc.pre_coast = DEFAULT_PRE_COAST;
    cm.cc.post_coast = DEFAULT_POST_COAST;
    tc.post_coast = DEFAULT_POST_COAST;
}

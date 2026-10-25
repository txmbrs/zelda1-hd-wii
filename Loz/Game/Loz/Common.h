// Wii port modifications, 2026, txmbrs and contributors.
/*
   Copyright 2016 Aldo J. Nunez

   Licensed under the Apache License, Version 2.0.
   See the LICENSE text file for details.
*/

#pragma once

#include "HAL.h"

#if defined(USE_ALLEGRO)
#include <allegro5/allegro5.h>
#if defined(_WIN32)
#include <allegro5/allegro_windows.h>
#endif
#endif

// This project
#include "Global.h"
#include "ObjType.h"
#include "Util.h"

#if defined(_WIN32)
#undef GetObject
#endif

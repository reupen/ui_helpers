#ifndef _UI_HELPERS_H_
#define _UI_HELPERS_H_

#define OEMRESOURCE

#include <algorithm>
#include <functional>
#include <memory>
#include <string>
#include <type_traits>
#include <vector>
#include <unordered_map>

#include <windows.h>
#include <WindowsX.h>
#include <SHLWAPI.H>
#include <vssym32.h>
#include <uxtheme.h>
#include <shldisp.h>
#include <ShlObj.h>
#include <gdiplus.h>
#include <Usp10.h>
#include <CommonControls.h>

// Windows SDK headers define min and max macros, which are used by gdiplus.h.
// 
// This undefines them temporarily to avoid conflicts with the C++ standard 
// library. They are redefined at the end of this header.
#ifdef max
#define _UI_HELPERS_OLD_MAX_
#undef max
#endif

#ifdef min
#define _UI_HELPERS_OLD_MIN_
#undef min
#endif

#include "../pfc/pfc.h"
#include "../mmh/stdafx.h"

#ifndef RECT_CX
#define RECT_CX(rc) (rc.right-rc.left)
#endif

#ifndef RECT_CY
#define RECT_CY(rc) (rc.bottom-rc.top)
#endif

#include "handle.h"
#include "win32_helpers.h"
#include "ole.h"

#include "container_window.h"
#include "message_hook.h"
#include "trackbar.h"
#include "solid_fill.h"

#include "gdi.h"
#include "text_drawing.h"
#include "list_view/list_view.h"
#include "drag_image.h"
#include "info_box.h"
#include "menu.h"

#include "ole/data_object.h"
#include "ole/enum_format_etc.h"


#ifdef _UI_HELPERS_OLD_MAX_
#define max(a,b)            (((a) > (b)) ? (a) : (b))
#undef _UI_HELPERS_OLD_MAX_
#endif

#ifdef _UI_HELPERS_OLD_MIN_
#define min(a,b)            (((a) < (b)) ? (a) : (b))
#undef _UI_HELPERS_OLD_MIN_
#endif

#endif
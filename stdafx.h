#ifndef _UI_HELPERS_H_
#define _UI_HELPERS_H_

#define OEMRESOURCE

#include <algorithm>
#include <functional>
#include <memory>
#include <optional>
#include <ranges>
#include <span>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <vector>

#include <ppl.h>

#include <gsl/gsl>
#include <range/v3/all.hpp>

// Included before windows.h, because pfc.h includes winsock2.h
#include "../pfc/pfc.h"

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
#include <intsafe.h>
#include <dwrite_3.h>

#include <wil/com.h>
#include <wil/resource.h>
#include <wil/win32_helpers.h>

#include "../mmh/stdafx.h"

#ifndef RECT_CX
#define RECT_CX(rc) ((rc).right - (rc).left)
#endif

#ifndef RECT_CY
#define RECT_CY(rc) ((rc).bottom - (rc).top)
#endif

#include "literals.h"
#include "event_token.h"

#include "handle.h"
#include "win32_helpers.h"
#include "ole.h"

#include "dialog.h"
#include "dpi.h"
#include "container_window.h"
#include "message_hook.h"
#include "trackbar.h"
#include "solid_fill.h"
#include "theming.h"

#include "gdi.h"
#include "uniscribe.h"
#include "uniscribe_text_out.h"
#include "text_out_helpers.h"
#include "direct_write.h"
#include "direct_write_text_out.h"
#include "list_view/list_view.h"
#include "drag_image.h"
#include "info_box.h"
#include "menu.h"
#include "window_subclasser.h"

#include "ole/data_object.h"
#include "ole/enum_format_etc.h"

#endif

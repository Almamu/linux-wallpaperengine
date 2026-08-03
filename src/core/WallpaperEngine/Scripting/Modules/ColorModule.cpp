#include "ColorModule.h"

#include "WallpaperEngine/Scripting/ScriptEngine.h"

using namespace WallpaperEngine::Scripting::Modules;

#define min_f(a, b, c) (fminf (a, fminf (b, c)))
#define max_f(a, b, c) (fmaxf (a, fmaxf (b, c)))

static uint32_t ColorModuleInstanceId = 0;
std::map<uint32_t, ColorModule&> colorModules;

int wecolor_init (JSContext* ctx, JSModuleDef* m) {

    JS_AddModuleExport (ctx, m, "rgb2hsv");
    JS_AddModuleExport (ctx, m, "hsv2rgb");
    JS_AddModuleExport (ctx, m, "normalizeColor");
    JS_AddModuleExport (ctx, m, "expandColor");

    return 0;
}

JSValue wecolor_rgb2hsv (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv, int magic) {
    if (argc != 1) {
	return JS_EXCEPTION;
    }

    if (JS_VALUE_GET_TAG (argv[0]) != JS_TAG_OBJECT) {
	return JS_EXCEPTION;
    }

    JSValue x = JS_GetPropertyStr (ctx, argv[0], "x");
    JSValue y = JS_GetPropertyStr (ctx, argv[0], "y");
    JSValue z = JS_GetPropertyStr (ctx, argv[0], "z");

    double xVal = 0.0f, yVal = 0.0f, zVal = 0.0f;

    JS_ToFloat64 (ctx, &xVal, x);
    JS_ToFloat64 (ctx, &yVal, y);
    JS_ToFloat64 (ctx, &zVal, z);

    // conversion code from https://gist.github.com/yoggy/8999625
    float h, s, v; // h:0-360.0, s:0.0-1.0, v:0.0-1.0

    float max = max_f (xVal, yVal, zVal);
    float min = min_f (xVal, yVal, zVal);

    v = max;

    if (max == 0.0f) {
	s = 0;
	h = 0;
    } else if (max - min == 0.0f) {
	s = 0;
	h = 0;
    } else {
	s = (max - min) / max;

	if (max == xVal) {
	    h = 60 * ((yVal - zVal) / (max - min)) + 0;
	} else if (max == yVal) {
	    h = 60 * ((zVal - xVal) / (max - min)) + 120;
	} else {
	    h = 60 * ((xVal - yVal) / (max - min)) + 240;
	}
    }

    if (h < 0) {
	h += 360.0f;
    }

    const auto it = colorModules.find (magic);

    if (it == colorModules.end ()) {
	return JS_UNDEFINED;
    }

    JSValue value = it->second.getEngine ().getAdapters ().vec3->instantiate ();

    JS_SetPropertyStr (ctx, value, "x", JS_NewFloat64 (ctx, h));
    JS_SetPropertyStr (ctx, value, "y", JS_NewFloat64 (ctx, s));
    JS_SetPropertyStr (ctx, value, "z", JS_NewFloat64 (ctx, v));

    return value;
}

JSValue wecolor_hsv2rgb (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv, int magic) {
    if (argc != 1) {
	return JS_EXCEPTION;
    }

    if (JS_VALUE_GET_TAG (argv[0]) != JS_TAG_OBJECT) {
	return JS_EXCEPTION;
    }

    JSValue x = JS_GetPropertyStr (ctx, argv[0], "x");
    JSValue y = JS_GetPropertyStr (ctx, argv[0], "y");
    JSValue z = JS_GetPropertyStr (ctx, argv[0], "z");

    double xVal = 0.0f, yVal = 0.0f, zVal = 0.0f;

    JS_ToFloat64 (ctx, &xVal, x);
    JS_ToFloat64 (ctx, &yVal, y);
    JS_ToFloat64 (ctx, &zVal, z);

    // conversion code from https://gist.github.com/yoggy/8999625
    float r, g, b; // 0.0-1.0

    int hi = (int)(xVal / 60.0f) % 6;
    float f = (xVal / 60.0f) - hi;
    float p = zVal * (1.0f - yVal);
    float q = zVal * (1.0f - yVal * f);
    float t = zVal * (1.0f - yVal * (1.0f - f));

    switch (hi) {
	case 0:
	    r = zVal, g = t, b = p;
	    break;
	case 1:
	    r = q, g = zVal, b = p;
	    break;
	case 2:
	    r = p, g = zVal, b = t;
	    break;
	case 3:
	    r = p, g = q, b = zVal;
	    break;
	case 4:
	    r = t, g = p, b = zVal;
	    break;
	case 5:
	    r = zVal, g = p, b = q;
	    break;
	default:
	    r = 0, g = 0, b = 0;
	    break;
    }

    const auto it = colorModules.find (magic);

    if (it == colorModules.end ()) {
	return JS_UNDEFINED;
    }

    JSValue value = it->second.getEngine ().getAdapters ().vec3->instantiate ();

    JS_SetPropertyStr (ctx, value, "x", JS_NewFloat64 (ctx, r));
    JS_SetPropertyStr (ctx, value, "y", JS_NewFloat64 (ctx, g));
    JS_SetPropertyStr (ctx, value, "z", JS_NewFloat64 (ctx, b));

    return value;
}

JSValue wecolor_normalizecolor (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv, int magic) {
    if (argc != 1) {
	return JS_EXCEPTION;
    }

    if (JS_VALUE_GET_TAG (argv[0]) != JS_TAG_OBJECT) {
	return JS_EXCEPTION;
    }

    const auto it = colorModules.find (magic);

    if (it == colorModules.end ()) {
	return JS_UNDEFINED;
    }

    JSValue x = JS_GetPropertyStr (ctx, argv[0], "x");
    JSValue y = JS_GetPropertyStr (ctx, argv[0], "y");
    JSValue z = JS_GetPropertyStr (ctx, argv[0], "z");

    if (!JS_IsNumber (x) || !JS_IsNumber (y) || !JS_IsNumber (z)) {
	return JS_EXCEPTION;
    }

    double xVal = 0.0f, yVal = 0.0f, zVal = 0.0f;

    JS_ToFloat64 (ctx, &xVal, x);
    JS_ToFloat64 (ctx, &yVal, y);
    JS_ToFloat64 (ctx, &zVal, z);

    JSValue value = it->second.getEngine ().getAdapters ().vec3->instantiate ();

    JS_SetPropertyStr (ctx, value, "x", JS_NewFloat64 (ctx, xVal / 255.0f));
    JS_SetPropertyStr (ctx, value, "y", JS_NewFloat64 (ctx, yVal / 255.0f));
    JS_SetPropertyStr (ctx, value, "z", JS_NewFloat64 (ctx, zVal / 255.0f));

    return value;
}

JSValue wecolor_expandcolor (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv, int magic) {
    if (argc != 1) {
	return JS_EXCEPTION;
    }

    if (JS_VALUE_GET_TAG (argv[0]) != JS_TAG_OBJECT) {
	return JS_EXCEPTION;
    }

    const auto it = colorModules.find (magic);

    if (it == colorModules.end ()) {
	return JS_UNDEFINED;
    }

    JSValue x = JS_GetPropertyStr (ctx, argv[0], "x");
    JSValue y = JS_GetPropertyStr (ctx, argv[0], "y");
    JSValue z = JS_GetPropertyStr (ctx, argv[0], "z");

    if (!JS_IsNumber (x) || !JS_IsNumber (y) || !JS_IsNumber (z)) {
	return JS_EXCEPTION;
    }

    double xVal = 0.0f, yVal = 0.0f, zVal = 0.0f;

    JS_ToFloat64 (ctx, &xVal, x);
    JS_ToFloat64 (ctx, &yVal, y);
    JS_ToFloat64 (ctx, &zVal, z);

    JSValue value = it->second.getEngine ().getAdapters ().vec3->instantiate ();

    JS_SetPropertyStr (ctx, value, "x", JS_NewFloat64 (ctx, xVal * 255.0f));
    JS_SetPropertyStr (ctx, value, "y", JS_NewFloat64 (ctx, yVal * 255.0f));
    JS_SetPropertyStr (ctx, value, "z", JS_NewFloat64 (ctx, zVal * 255.0f));

    return value;
}

ColorModule::ColorModule (ScriptEngine& engine) : ScriptModule (engine, "WEColor", wecolor_init) {
    this->m_instanceId = ++ColorModuleInstanceId;

    JS_SetModuleExport (
	this->getEngine ().getContext (), this->getDefinition (), "rgb2hsv",
	JS_NewCFunctionMagic (
	    this->getEngine ().getContext (), wecolor_rgb2hsv, "rgb2hsv", 1, JS_CFUNC_generic_magic, this->m_instanceId
	)
    );

    JS_SetModuleExport (
	this->getEngine ().getContext (), this->getDefinition (), "hsv2rgb",
	JS_NewCFunctionMagic (
	    this->getEngine ().getContext (), wecolor_hsv2rgb, "hsv2rgb", 1, JS_CFUNC_generic_magic, this->m_instanceId
	)
    );

    JS_SetModuleExport (
	this->getEngine ().getContext (), this->getDefinition (), "normalizeColor",
	JS_NewCFunctionMagic (
	    this->getEngine ().getContext (), wecolor_normalizecolor, "normalizeColor", 1, JS_CFUNC_generic_magic,
	    this->m_instanceId
	)
    );

    JS_SetModuleExport (
	this->getEngine ().getContext (), this->getDefinition (), "expandColor",
	JS_NewCFunctionMagic (
	    this->getEngine ().getContext (), wecolor_expandcolor, "expandColor", 1, JS_CFUNC_generic_magic,
	    this->m_instanceId
	)
    );

    colorModules.emplace (this->m_instanceId, *this);
}

ColorModule::~ColorModule () { colorModules.erase (this->m_instanceId); }
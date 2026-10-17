#include "MathModule.h"

#include "WallpaperEngine/Scripting/ScriptEngine.h"

using namespace WallpaperEngine::Scripting::Modules;

#define min_f(a, b, c) (fminf (a, fminf (b, c)))
#define max_f(a, b, c) (fmaxf (a, fmaxf (b, c)))

JSValue wemath_smoothstep (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
    if (argc != 3) {
	return JS_EXCEPTION;
    }

    if (!JS_IsNumber (argv[0]) || !JS_IsNumber (argv[1]) || !JS_IsNumber (argv[2])) {
	return JS_EXCEPTION;
    }

    double edge0 = 0.0f;
    double edge1 = 1.0f;
    double x = 0.0f;

    JS_ToFloat64 (ctx, &edge0, argv[0]);
    JS_ToFloat64 (ctx, &edge1, argv[1]);
    JS_ToFloat64 (ctx, &x, argv[2]);

    return JS_NewFloat64 (ctx, glm::smoothstep (edge0, edge1, x));
}

JSValue wemath_mix (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
    if (argc != 3) {
	return JS_EXCEPTION;
    }

    if (!JS_IsNumber (argv[0]) || !JS_IsNumber (argv[1]) || !JS_IsNumber (argv[2])) {
	return JS_EXCEPTION;
    }

    double a = 0.0f;
    double b = 1.0f;
    double value = 0.0f;

    JS_ToFloat64 (ctx, &a, argv[0]);
    JS_ToFloat64 (ctx, &b, argv[1]);
    JS_ToFloat64 (ctx, &value, argv[2]);

    return JS_NewFloat64 (ctx, glm::mix (a, b, value));
}

int wemath_init (JSContext* ctx, JSModuleDef* m) {
	if (JS_SetModuleExport (ctx, m, "smoothStep", JS_NewCFunction (ctx, wemath_smoothstep, "smoothStep", 3)) < 0
	|| JS_SetModuleExport (ctx, m, "mix", JS_NewCFunction (ctx, wemath_mix, "mix", 3)) < 0
	|| JS_SetModuleExport (ctx, m, "deg2rad", JS_NewFloat64 (ctx, 0.01745329251994329576923690768489)) < 0
	|| JS_SetModuleExport (ctx, m, "rad2deg", JS_NewFloat64 (ctx, 57.295779513082320876798154814105)) < 0) {
	return -1;
    }

    return 0;
}

MathModule::MathModule (ScriptEngine& engine) : ScriptModule (engine, "WEMath", wemath_init) {
	JS_AddModuleExport (this->getEngine ().getContext (), this->getDefinition (), "smoothStep");
	JS_AddModuleExport (this->getEngine ().getContext (), this->getDefinition (), "mix");
	JS_AddModuleExport (this->getEngine ().getContext (), this->getDefinition (), "deg2rad");
	JS_AddModuleExport (this->getEngine ().getContext (), this->getDefinition (), "rad2deg");
}

MathModule::~MathModule () = default;

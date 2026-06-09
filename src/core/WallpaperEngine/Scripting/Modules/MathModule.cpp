#include "MathModule.h"

#include "WallpaperEngine/Scripting/ScriptEngine.h"

using namespace WallpaperEngine::Scripting::Modules;

#define min_f(a, b, c) (fminf (a, fminf (b, c)))
#define max_f(a, b, c) (fmaxf (a, fmaxf (b, c)))

static uint32_t MathModuleInstanceId = 0;
std::map<uint32_t, MathModule&> mathModules;

int wemath_init (JSContext* ctx, JSModuleDef* m) {

    JS_AddModuleExport (ctx, m, "smoothStep");
    JS_AddModuleExport (ctx, m, "mix");
    JS_AddModuleExport (ctx, m, "deg2rad");
    JS_AddModuleExport (ctx, m, "rad2deg");

    return 0;
}

JSValue wemath_smoothstep (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv, int magic) {
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

JSValue wemath_mix (JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv, int magic) {
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

MathModule::MathModule (ScriptEngine& engine) : ScriptModule (engine, "WEMath", wemath_init) {
    this->m_instanceId = ++MathModuleInstanceId;

    JS_SetModuleExport (
	this->getEngine ().getContext (), this->getDefinition (), "smoothStep",
	JS_NewCFunctionMagic (
	    this->getEngine ().getContext (), wemath_smoothstep, "smoothStep", 3, JS_CFUNC_generic_magic,
	    this->m_instanceId
	)
    );

    JS_SetModuleExport (
	this->getEngine ().getContext (), this->getDefinition (), "mix",
	JS_NewCFunctionMagic (
	    this->getEngine ().getContext (), wemath_mix, "mix", 1, JS_CFUNC_generic_magic, this->m_instanceId
	)
    );

    JS_SetModuleExport (
	this->getEngine ().getContext (), this->getDefinition (), "deg2rad",
	JS_NewFloat64 (this->getEngine ().getContext (), 0.01745329251994329576923690768489)
    );

    JS_SetModuleExport (
	this->getEngine ().getContext (), this->getDefinition (), "rad2deg",
	JS_NewFloat64 (this->getEngine ().getContext (), 57.295779513082320876798154814105)
    );

    mathModules.emplace (this->m_instanceId, *this);
}

MathModule::~MathModule () { mathModules.erase (this->m_instanceId); }
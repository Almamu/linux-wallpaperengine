#include "GLSLContext.h"
#include "WallpaperEngine/Logging/Log.h"

#include <algorithm>
#include <cassert>
#include <cctype>
#include <cstdlib>
#include <fstream>
#include <map>
#include <memory>
#include <regex>
#include <set>
#include <string_view>
#include <unistd.h>

#include "SPIRV/GlslangToSpv.h"
#include "glslang/Include/ResourceLimits.h"
#include "glslang/Public/ShaderLang.h"
#include "spirv_glsl.hpp"

using namespace WallpaperEngine::Render::Shaders;

namespace {
std::string stripCommentsPreservingOffsets (const std::string& source) {
    std::string result = source;
    bool lineComment = false;
    bool blockComment = false;
    for (std::size_t i = 0; i < result.size (); ++i) {
	if (lineComment) {
	    if (result[i] == '\n') {
		lineComment = false;
	    } else {
		result[i] = ' ';
	    }
	} else if (blockComment) {
	    if (result[i] == '*' && i + 1 < result.size () && result[i + 1] == '/') {
		result[i] = result[i + 1] = ' ';
		++i;
		blockComment = false;
	    } else if (result[i] != '\n') {
		result[i] = ' ';
	    }
	} else if (result[i] == '/' && i + 1 < result.size () && result[i + 1] == '/') {
	    result[i] = result[i + 1] = ' ';
	    ++i;
	    lineComment = true;
	} else if (result[i] == '/' && i + 1 < result.size () && result[i + 1] == '*') {
	    result[i] = result[i + 1] = ' ';
	    ++i;
	    blockComment = true;
	}
    }
    return result;
}

/**
 * Some shared Workshop effects ship a stray #endif in one shader stage. Drop
 * only closing directives that have no matching #if/#ifdef/#ifndef, while
 * preserving comments, source offsets, and all balanced conditional blocks.
 */
std::string neutralizeUnmatchedEndifs (const std::string& source, std::vector<std::size_t>& lines) {
    const std::string uncommented = stripCommentsPreservingOffsets (source);
    std::string result = source;
    int conditionalDepth = 0;

    const std::regex directive (R"(^\s*#\s*(if|ifdef|ifndef|elif|else|endif)\b)");
    for (std::size_t lineStart = 0; lineStart < uncommented.size ();) {
	const std::size_t lineEnd = uncommented.find ('\n', lineStart);
	const std::size_t lineLength = (lineEnd == std::string::npos ? uncommented.size () : lineEnd) - lineStart;
	const std::string line = uncommented.substr (lineStart, lineLength);
	std::smatch match;
	if (std::regex_search (line, match, directive, std::regex_constants::match_continuous)) {
	    const std::string kind = match[1].str ();
	    if (kind == "if" || kind == "ifdef" || kind == "ifndef") {
		++conditionalDepth;
	    } else if (kind == "endif") {
		if (conditionalDepth > 0) {
		    --conditionalDepth;
		} else {
		    const std::size_t hash = lineStart + line.find ('#');
		    result[hash] = '/';
		    if (hash + 1 < result.size ()) {
			result[hash + 1] = '/';
		    }
		    lines.push_back (
			static_cast<std::size_t> (
			    std::count (uncommented.begin (), uncommented.begin () + lineStart, '\n')
			)
			+ 1
		    );
		}
	    }
	}
	if (lineEnd == std::string::npos) {
	    break;
	}
	lineStart = lineEnd + 1;
    }

    return result;
}

/**
 * A few workshop effect vertex shaders declare a vec4 varying but only ever
 * write its .xy components, while the paired fragment shader declares vec2.
 * Narrowing that output is semantics-preserving and gives both linked stages
 * the same interface type. Leave any varying with z/w uses untouched.
 */
bool varyingUseIsXyOnly (const std::string& code, std::size_t start, std::size_t length) {
    std::size_t suffix = start + length;
    while (suffix < code.size () && std::isspace (static_cast<unsigned char> (code[suffix]))) {
	++suffix;
    }
    if (suffix + 3 > code.size () || code.compare (suffix, 3, ".xy") != 0) {
	return false;
    }
    if (suffix + 3 == code.size ()) {
	return true;
    }
    const unsigned char next = static_cast<unsigned char> (code[suffix + 3]);
    return !std::isalnum (next) && code[suffix + 3] != '_';
}

bool varyingHasOnlyXyUses (
    const std::string& code, const std::string& name, std::size_t declarationStart, std::size_t declarationEnd
) {
    const std::regex identifier ("\\b" + name + "\\b");
    bool hasUse = false;
    for (std::sregex_iterator use (code.begin (), code.end (), identifier), end; use != end; ++use) {
	const auto useStart = static_cast<std::size_t> (use->position ());
	if (useStart >= declarationStart && useStart < declarationEnd) {
	    continue;
	}
	hasUse = true;
	if (!varyingUseIsXyOnly (code, useStart, static_cast<std::size_t> (use->length ()))) {
	    return false;
	}
    }
    return hasUse;
}

std::string normalizeXyOnlyVaryingMismatches (
    const std::string& vertex, const std::string& fragment, std::vector<std::string>& adjusted
) {
    const std::string vertexCode = stripCommentsPreservingOffsets (vertex);
    const std::string fragmentCode = stripCommentsPreservingOffsets (fragment);
    const std::regex vertexVec4Declaration (R"(\bvarying\s+vec4\s+([A-Za-z_]\w*)\s*;)");
    std::vector<std::pair<std::size_t, std::size_t>> declarations;

    for (std::sregex_iterator it (vertexCode.begin (), vertexCode.end (), vertexVec4Declaration), end; it != end;
	 ++it) {
	const std::string name = (*it)[1].str ();
	const std::regex fragmentVec2Declaration ("\\bvarying\\s+vec2\\s+" + name + "\\s*;");
	if (!std::regex_search (fragmentCode, fragmentVec2Declaration)) {
	    continue;
	}

	const std::size_t declarationStart = static_cast<std::size_t> (it->position ());
	const std::size_t declarationEnd = declarationStart + static_cast<std::size_t> (it->length ());
	if (!varyingHasOnlyXyUses (vertexCode, name, declarationStart, declarationEnd)) {
	    continue;
	}

	const std::size_t typeStart = declarationStart + it->str ().find ("vec4");
	declarations.emplace_back (typeStart, 4);
	adjusted.push_back (name);
    }

    std::string result = vertex;
    for (auto it = declarations.rbegin (); it != declarations.rend (); ++it) {
	result.replace (it->first, it->second, "vec2");
    }
    return result;
}

struct ConditionalBranchState {
    std::vector<std::string> branches;
    std::string active;
};

std::string trimHorizontalWhitespace (const std::string& value) {
    const std::size_t first = value.find_first_not_of (" \t\r");
    if (first == std::string::npos) {
	return "";
    }
    const std::size_t last = value.find_last_not_of (" \t\r");
    return value.substr (first, last - first + 1);
}

std::string joinedBranches (const std::vector<std::string>& branches) {
    std::string result;
    for (const auto& branch : branches) {
	if (!result.empty ()) {
	    result += " || ";
	}
	result += "(" + branch + ")";
    }
    return result;
}

std::string normalizeDirectiveExpression (const std::string& kind, const std::string& rawExpression) {
    const std::string expression = trimHorizontalWhitespace (rawExpression);
    if (kind == "ifdef") {
	return "defined(" + expression + ")";
    }
    if (kind == "ifndef") {
	return "!defined(" + expression + ")";
    }
    return expression;
}

void applyConditionalDirective (
    const std::string& kind, const std::string& expression, std::vector<ConditionalBranchState>& stack
) {
    if (kind == "if" || kind == "ifdef" || kind == "ifndef") {
	stack.push_back ({ { expression }, expression });
	return;
    }
    if (stack.empty ()) {
	return;
    }
    if (kind == "elif") {
	stack.back ().active = "!(" + joinedBranches (stack.back ().branches) + ") && (" + expression + ")";
	stack.back ().branches.push_back (expression);
    } else if (kind == "else") {
	stack.back ().active = "!(" + joinedBranches (stack.back ().branches) + ")";
    } else if (kind == "endif") {
	stack.pop_back ();
    }
}

std::string activeConditionalExpression (const std::vector<ConditionalBranchState>& stack) {
    std::string condition;
    for (const auto& conditional : stack) {
	if (!condition.empty ()) {
	    condition += " && ";
	}
	condition += "(" + conditional.active + ")";
    }
    return condition;
}

std::string preprocessorConditionAt (const std::string& source, const std::size_t offset) {
    const std::string code = stripCommentsPreservingOffsets (source);
    std::vector<ConditionalBranchState> stack;
    const std::regex directive (R"(^\s*#\s*(if|ifdef|ifndef|elif|else|endif)\b(.*)$)");

    for (std::size_t lineStart = 0; lineStart < offset && lineStart < code.size ();) {
	const std::size_t lineEnd = code.find ('\n', lineStart);
	const std::size_t end = lineEnd == std::string::npos ? code.size () : lineEnd;
	std::string line = code.substr (lineStart, end - lineStart);
	if (!line.empty () && line.back () == '\r') {
	    line.pop_back ();
	}

	std::smatch match;
	if (std::regex_search (line, match, directive, std::regex_constants::match_continuous)) {
	    const std::string kind = match[1].str ();
	    const std::string expression = normalizeDirectiveExpression (kind, match[2].str ());
	    applyConditionalDirective (kind, expression, stack);
	}

	if (lineEnd == std::string::npos || lineEnd + 1 >= offset) {
	    break;
	}
	lineStart = lineEnd + 1;
    }
    return activeConditionalExpression (stack);
}

/**
 * A few Workshop effect shaders mutate a fragment-stage varying (most often
 * v_TexCoord) while transforming texture coordinates. Fragment inputs are
 * read-only in GLSL, so move that varying interface to a private linked name
 * and initialize a mutable per-fragment copy before main's logic runs.
 */
struct MutableVarying {
    std::size_t declarationStart;
    std::size_t declarationLength;
    std::string type;
    std::string name;
    std::string condition;
};

std::vector<MutableVarying> findMutableFragmentVaryings (
    const std::string& vertex, const std::string& fragment, std::vector<std::string>& adjusted
) {
    const std::regex fragmentVarying (R"(\bvarying\s+([A-Za-z_]\w*)\s+([A-Za-z_]\w*)\s*;)");
    const std::string fragmentCode = stripCommentsPreservingOffsets (fragment);
    const std::string vertexCode = stripCommentsPreservingOffsets (vertex);
    std::vector<MutableVarying> mutableVaryings;
    std::set<std::string> mutableNames;

    for (std::sregex_iterator it (fragmentCode.begin (), fragmentCode.end (), fragmentVarying), end; it != end; ++it) {
	const std::string type = (*it)[1].str ();
	const std::string name = (*it)[2].str ();
	const std::regex vertexVarying ("\\bvarying\\s+" + type + "\\s+" + name + "\\s*;");
	if (!std::regex_search (vertexCode, vertexVarying)) {
	    continue;
	}

	const std::regex writeAccess (
	    "\\b" + name + "\\b\\s*(?:(?:\\.[xyzwrgba]{1,4})|(?:\\[[^\\]]+\\]))?\\s*(?:\\+\\+|--|[+\\-*/]?=(?!=))"
	);
	if (!std::regex_search (fragmentCode, writeAccess)) {
	    continue;
	}

	const std::string alias = "_lweInput_" + name;
	const std::regex aliasIdentifier ("\\b" + alias + "\\b");
	if (std::regex_search (fragmentCode, aliasIdentifier) || std::regex_search (vertex, aliasIdentifier)) {
	    continue;
	}

	mutableVaryings.push_back (
	    { static_cast<std::size_t> (it->position ()), static_cast<std::size_t> (it->length ()), type, name,
	      preprocessorConditionAt (fragment, static_cast<std::size_t> (it->position ())) }
	);
	if (mutableNames.insert (name).second) {
	    adjusted.push_back (name);
	}
    }

    std::sort (mutableVaryings.begin (), mutableVaryings.end (), [] (const auto& lhs, const auto& rhs) {
	return lhs.declarationStart > rhs.declarationStart;
    });
    return mutableVaryings;
}

void rewriteMutableFragmentVaryings (
    std::string& vertex, std::string& fragment, const std::vector<MutableVarying>& mutableVaryings
) {
    std::set<std::string> vertexAliasesApplied;
    std::set<std::pair<std::string, std::string>> localDeclarations;
    for (const auto& varying : mutableVaryings) {
	const std::string alias = "_lweInput_" + varying.name;
	if (vertexAliasesApplied.insert (varying.name).second) {
	    const std::regex identifier ("\\b" + varying.name + "\\b");
	    vertex = std::regex_replace (vertex, identifier, alias);
	}

	std::string replacement = "varying " + varying.type + " " + alias + ";";
	if (localDeclarations.emplace (varying.name, varying.condition).second) {
	    replacement += "\n" + varying.type + " " + varying.name + ";";
	}
	fragment.replace (varying.declarationStart, varying.declarationLength, replacement);
    }
}

std::string mutableVaryingInitialization (const std::vector<MutableVarying>& mutableVaryings) {
    std::string initialization;
    std::set<std::pair<std::string, std::string>> initialized;
    for (const auto& varying : mutableVaryings) {
	if (!initialized.emplace (varying.name, varying.condition).second) {
	    continue;
	}
	if (!varying.condition.empty ()) {
	    initialization += "\n#if " + varying.condition + "\n";
	}
	initialization += "    " + varying.name + " = _lweInput_" + varying.name + ";\n";
	if (!varying.condition.empty ()) {
	    initialization += "#endif\n";
	}
    }
    return initialization;
}

void injectMutableVaryingInitializers (std::string& fragment, const std::vector<MutableVarying>& mutableVaryings) {
    const std::string fragmentCode = stripCommentsPreservingOffsets (fragment);
    const std::regex mainFunction (R"(\bvoid\s+main\s*\(\s*\)\s*\{)");
    std::vector<std::size_t> mainBodies;
    for (std::sregex_iterator it (fragmentCode.begin (), fragmentCode.end (), mainFunction), end; it != end; ++it) {
	mainBodies.push_back (static_cast<std::size_t> (it->position () + it->length () - 1));
    }
    std::sort (mainBodies.rbegin (), mainBodies.rend ());

    const std::string initialization = mutableVaryingInitialization (mutableVaryings);
    for (const std::size_t bodyStart : mainBodies) {
	fragment.insert (bodyStart + 1, initialization);
    }
}

std::string
normalizeWritableFragmentVaryings (std::string& vertex, std::string fragment, std::vector<std::string>& adjusted) {
    const auto mutableVaryings = findMutableFragmentVaryings (vertex, fragment, adjusted);
    if (mutableVaryings.empty ()) {
	return fragment;
    }

    rewriteMutableFragmentVaryings (vertex, fragment, mutableVaryings);
    injectMutableVaryingInitializers (fragment, mutableVaryings);
    return fragment;
}

/**
 * Shared blending helpers accept a scalar opacity, but some workshop effects
 * pass a vec2 temporary derived from their UVs. In that call context the first
 * component is the scalar coverage value the helper can consume.
 */
std::vector<std::pair<std::size_t, std::size_t>> callArguments (const std::string& code, std::size_t open);

std::string scalarizeVectorNames (
    std::string expression, const std::set<std::string>& vectorNames, std::vector<std::string>& adjusted
) {
    for (const auto& name : vectorNames) {
	const std::regex vectorUse ("\\b" + name + "\\b(?!\\s*\\.)");
	const std::string scalarized = std::regex_replace (expression, vectorUse, name + ".x");
	if (scalarized != expression) {
	    expression = scalarized;
	    adjusted.push_back (name);
	}
    }
    return expression;
}

std::string normalizeApplyBlendingVectorOpacity (std::string source, std::vector<std::string>& adjusted) {
    const std::string code = stripCommentsPreservingOffsets (source);
    const std::regex scalarApplyBlending (
	R"(\bApplyBlending\s*\(\s*(?:const\s+)?int\s+\w+\s*,\s*(?:(?:in|out|inout)\s+)?vec3\s+\w+\s*,\s*(?:(?:in|out|inout)\s+)?vec3\s+\w+\s*,\s*(?:(?:in|out|inout)\s+)?float\s+\w+)"
    );
    if (!std::regex_search (code, scalarApplyBlending)) {
	return source;
    }

    std::set<std::string> vectorNames;
    const std::regex vectorDeclaration (R"(\bvec[234]\s+([A-Za-z_]\w*)\b)");
    for (std::sregex_iterator it (code.begin (), code.end (), vectorDeclaration), end; it != end; ++it) {
	vectorNames.insert ((*it)[1].str ());
    }

    struct Replacement {
	std::size_t start;
	std::size_t length;
	std::string expression;
    };
    std::vector<Replacement> replacements;
    const std::regex functionName (R"(\bApplyBlending\s*\()");
    for (std::sregex_iterator it (code.begin (), code.end (), functionName), end; it != end; ++it) {
	const std::size_t open = static_cast<std::size_t> (it->position () + it->length () - 1);
	const auto arguments = callArguments (code, open);
	if (arguments.size () != 4) {
	    continue;
	}

	const auto [opacityStart, opacityEnd] = arguments[3];
	const std::string opacity = code.substr (opacityStart, opacityEnd - opacityStart);
	const std::string scalarized = scalarizeVectorNames (opacity, vectorNames, adjusted);
	if (scalarized != opacity) {
	    replacements.push_back ({ opacityStart, opacityEnd - opacityStart, scalarized });
	}
    }

    std::sort (replacements.begin (), replacements.end (), [] (const auto& lhs, const auto& rhs) {
	return lhs.start > rhs.start;
    });
    for (const auto& replacement : replacements) {
	source.replace (replacement.start, replacement.length, replacement.expression);
    }
    return source;
}

std::vector<std::pair<std::size_t, std::size_t>> callArguments (const std::string& code, const std::size_t open) {
    std::vector<std::pair<std::size_t, std::size_t>> arguments;
    std::size_t argumentStart = open + 1;
    int parenDepth = 0;
    int bracketDepth = 0;
    int braceDepth = 0;
    for (std::size_t current = open + 1; current < code.size (); ++current) {
	if (code[current] == '(') {
	    ++parenDepth;
	} else if (code[current] == ')') {
	    if (parenDepth == 0 && bracketDepth == 0 && braceDepth == 0) {
		arguments.emplace_back (argumentStart, current);
		break;
	    }
	    --parenDepth;
	} else if (code[current] == '[') {
	    ++bracketDepth;
	} else if (code[current] == ']') {
	    --bracketDepth;
	} else if (code[current] == '{') {
	    ++braceDepth;
	} else if (code[current] == '}') {
	    --braceDepth;
	} else if (code[current] == ',' && parenDepth == 0 && bracketDepth == 0 && braceDepth == 0) {
	    arguments.emplace_back (argumentStart, current);
	    argumentStart = current + 1;
	}
    }
    return arguments;
}

std::size_t expressionEnd (const std::string& code, const std::size_t start) {
    int parenDepth = 0;
    int bracketDepth = 0;
    int braceDepth = 0;
    for (std::size_t current = start; current < code.size (); ++current) {
	if (code[current] == '(') {
	    ++parenDepth;
	} else if (code[current] == ')') {
	    --parenDepth;
	} else if (code[current] == '[') {
	    ++bracketDepth;
	} else if (code[current] == ']') {
	    --bracketDepth;
	} else if (code[current] == '{') {
	    ++braceDepth;
	} else if (code[current] == '}') {
	    --braceDepth;
	} else if (code[current] == ';' && parenDepth == 0 && bracketDepth == 0 && braceDepth == 0) {
	    return current;
	}
    }
    return code.size ();
}

/**
 * Effect uniforms such as g_Texture0Resolution intentionally carry four
 * dimensions. When an expression is explicitly used as a vec2 value (or as
 * 2D texture coordinates), use its XY dimensions at that use site instead of
 * changing the uniform's shared ABI.
 */
struct Vec2Replacement {
    std::size_t start;
    std::size_t length;
    std::string value;
};

void collectVec4Operands (
    const std::string& code, const std::set<std::string>& vec4Names, std::size_t start, std::size_t end,
    std::vector<Vec2Replacement>& replacements, std::vector<std::string>& adjusted
) {
    const std::string expression = code.substr (start, end - start);
    const std::regex identifier (R"(\b[A-Za-z_]\w*\b)");
    for (std::sregex_iterator it (expression.begin (), expression.end (), identifier), finish; it != finish; ++it) {
	const std::string name = it->str ();
	if (!vec4Names.contains (name)) {
	    continue;
	}

	const std::size_t localStart = static_cast<std::size_t> (it->position ());
	const std::size_t localEnd = localStart + static_cast<std::size_t> (it->length ());
	std::size_t suffix = localEnd;
	while (suffix < expression.size () && std::isspace (static_cast<unsigned char> (expression[suffix]))) {
	    ++suffix;
	}
	std::size_t prefix = localStart;
	while (prefix > 0 && std::isspace (static_cast<unsigned char> (expression[prefix - 1]))) {
	    --prefix;
	}
	if ((suffix < expression.size () && expression[suffix] == '.')
	    || (prefix > 0 && expression[prefix - 1] == '.')) {
	    continue;
	}

	replacements.push_back ({ start + localStart, localEnd - localStart, name + ".xy" });
	adjusted.push_back (name);
    }
}

std::string normalizeVec4OperandsInVec2Contexts (const std::string& source, std::vector<std::string>& adjusted) {
    const std::string code = stripCommentsPreservingOffsets (source);
    const std::regex typedName (
	R"(\b(float|int|uint|bool|vec[234]|[biu]vec[234]|mat[234](?:x[234])?)\s+([A-Za-z_]\w*))"
    );
    std::set<std::string> vec4Names;
    std::set<std::string> conflictingNames;
    for (std::sregex_iterator it (code.begin (), code.end (), typedName), end; it != end; ++it) {
	const std::string type = (*it)[1].str ();
	const std::string name = (*it)[2].str ();
	(type == "vec4" ? vec4Names : conflictingNames).insert (name);
    }
    for (const auto& name : conflictingNames) {
	vec4Names.erase (name);
    }
    if (vec4Names.empty ()) {
	return source;
    }

    std::vector<Vec2Replacement> replacements;
    const std::regex vec2Initializer (R"(\bvec2\s+[A-Za-z_]\w*\s*=\s*)");
    for (std::sregex_iterator it (code.begin (), code.end (), vec2Initializer), end; it != end; ++it) {
	const std::size_t start = static_cast<std::size_t> (it->position () + it->length ());
	collectVec4Operands (code, vec4Names, start, expressionEnd (code, start), replacements, adjusted);
    }

    const std::regex textureCall (R"(\b(?:texSample2D|texture|texture2D|texSample2DLod|textureLod|texture2DLod)\s*\()");
    for (std::sregex_iterator it (code.begin (), code.end (), textureCall), end; it != end; ++it) {
	const std::size_t open = static_cast<std::size_t> (it->position () + it->length () - 1);
	const auto arguments = callArguments (code, open);
	if (arguments.size () >= 2) {
	    collectVec4Operands (code, vec4Names, arguments[1].first, arguments[1].second, replacements, adjusted);
	}
    }

    std::sort (replacements.begin (), replacements.end (), [] (const auto& lhs, const auto& rhs) {
	return lhs.start > rhs.start;
    });
    replacements.erase (
	std::unique (
	    replacements.begin (), replacements.end (),
	    [] (const auto& lhs, const auto& rhs) { return lhs.start == rhs.start; }
	),
	replacements.end ()
    );

    std::string result = source;
    for (const auto& replacement : replacements) {
	result.replace (replacement.start, replacement.length, replacement.value);
    }
    return result;
}

/** Older Workshop ports use 0.0/1.0 floats as ternary conditions. */
std::string normalizeFloatConditions (const std::string& source, std::vector<std::string>& adjusted) {
    const std::string code = stripCommentsPreservingOffsets (source);
    const std::regex typedName (R"(\b(float|bool|int|uint|vec[234]|[biu]vec[234])\s+([A-Za-z_]\w*))");
    std::set<std::string> floatNames;
    std::set<std::string> conflictingNames;
    for (std::sregex_iterator it (code.begin (), code.end (), typedName), end; it != end; ++it) {
	const std::string type = (*it)[1].str ();
	const std::string name = (*it)[2].str ();
	if (type == "float") {
	    floatNames.insert (name);
	} else {
	    conflictingNames.insert (name);
	}
    }
    for (const auto& name : conflictingNames) {
	floatNames.erase (name);
    }

    struct Replacement {
	std::size_t start;
	std::size_t length;
	std::string value;
    };
    std::vector<Replacement> replacements;
    for (const auto& name : floatNames) {
	const std::regex ternary ("\\b" + name + R"(\s*\?)");
	for (std::sregex_iterator it (code.begin (), code.end (), ternary), end; it != end; ++it) {
	    const std::size_t start = static_cast<std::size_t> (it->position ());
	    if (start > 0
		&& (code[start - 1] == '.' || std::isalnum (static_cast<unsigned char> (code[start - 1]))
		    || code[start - 1] == '_')) {
		continue;
	    }
	    replacements.push_back ({ start, name.size (), "(" + name + " != 0.0)" });
	    adjusted.push_back (name);
	}
	const std::regex conditional ("\\b(?:if|while)\\s*\\(\\s*(" + name + R"()\s*\))");
	for (std::sregex_iterator it (code.begin (), code.end (), conditional), end; it != end; ++it) {
	    const std::size_t start = static_cast<std::size_t> (it->position (1));
	    replacements.push_back ({ start, name.size (), "(" + name + " != 0.0)" });
	    adjusted.push_back (name);
	}
    }
    std::sort (replacements.begin (), replacements.end (), [] (const auto& lhs, const auto& rhs) {
	return lhs.start > rhs.start;
    });
    replacements.erase (
	std::unique (
	    replacements.begin (), replacements.end (),
	    [] (const auto& lhs, const auto& rhs) { return lhs.start == rhs.start; }
	),
	replacements.end ()
    );
    std::string result = source;
    for (const auto& replacement : replacements) {
	result.replace (replacement.start, replacement.length, replacement.value);
    }
    return result;
}

std::string ensurePiFallback (const std::string& source) {
    const std::string fallback = "#ifndef M_PI\n#define M_PI 3.14159265358979323846\n#endif\n";
    const std::string uncommented = stripCommentsPreservingOffsets (source);
    const std::regex versionDirective (R"(^\s*#\s*version\b[^\n]*(?:\n|$))");
    std::smatch match;
    if (std::regex_search (uncommented, match, versionDirective, std::regex_constants::match_continuous)) {
	std::string result = source;
	result.insert (static_cast<std::size_t> (match.length ()), fallback);
	return result;
    }
    return fallback + source;
}

/** Preserve sequential macro meaning when included Workshop headers reuse pi names. */
std::string normalizePiMacroRedefinitions (const std::string& source) {
    const std::string uncommented = stripCommentsPreservingOffsets (source);
    const std::regex directive (R"(^\s*#\s*(define|undef)\s+(M_PI(?:_2)?)\b(.*)$)");
    std::map<std::string, std::string> definitions;
    std::vector<std::pair<std::size_t, std::string>> insertions;
    for (std::size_t lineStart = 0; lineStart < uncommented.size ();) {
	const std::size_t lineEnd = uncommented.find ('\n', lineStart);
	const std::size_t end = lineEnd == std::string::npos ? uncommented.size () : lineEnd;
	std::string logicalLine = uncommented.substr (lineStart, end - lineStart);
	std::size_t logicalEnd = end;
	while (!logicalLine.empty ()) {
	    std::size_t last = logicalLine.find_last_not_of (" \t\r");
	    if (last == std::string::npos || logicalLine[last] != '\\' || logicalEnd >= uncommented.size ()) {
		break;
	    }
	    const std::size_t nextEnd = uncommented.find ('\n', logicalEnd + 1);
	    const std::size_t nextLineEnd = nextEnd == std::string::npos ? uncommented.size () : nextEnd;
	    logicalLine += uncommented.substr (logicalEnd + 1, nextLineEnd - logicalEnd - 1);
	    logicalEnd = nextLineEnd;
	}
	if (!logicalLine.empty () && logicalLine.back () == '\r') {
	    logicalLine.pop_back ();
	}
	std::smatch match;
	if (std::regex_search (logicalLine, match, directive, std::regex_constants::match_continuous)) {
	    const std::string operation = match[1].str ();
	    const std::string name = match[2].str ();
	    if (operation == "undef") {
		definitions.erase (name);
	    } else {
		std::string value = match[3].str ();
		const std::size_t first = value.find_first_not_of (" \t\r");
		const std::size_t last = value.find_last_not_of (" \t\r");
		value = first == std::string::npos ? "" : value.substr (first, last - first + 1);
		const auto previous = definitions.find (name);
		if (previous != definitions.end () && previous->second != value) {
		    insertions.emplace_back (lineStart, name);
		}
		definitions[name] = value;
	    }
	}
	if (lineEnd == std::string::npos) {
	    break;
	}
	lineStart = lineEnd + 1;
    }
    std::string result = source;
    for (auto it = insertions.rbegin (); it != insertions.rend (); ++it) {
	result.insert (it->first, "#undef " + it->second + "\n");
    }
    return result;
}

/**
 * Wallpaper Engine workshop shaders occasionally mark values that depend on
 * uniforms as `const`. GLSL only permits compile-time expressions for const
 * initializers, so those shaders fail before reaching the driver's compiler.
 * Preserve the author's expression and intent: turn a dynamic global const
 * into a macro expression and remove `const` from dynamic local values.
 * Other invalid const expressions are deliberately left alone so their
 * diagnostics are not hidden.
 */
std::string normalizeUniformDependentConstants (const std::string& source, std::vector<std::string>& adjusted) {
    std::set<std::string> uniformNames;
    std::string uncommented = source;
    bool lineComment = false;
    bool blockComment = false;
    for (std::size_t i = 0; i < uncommented.size (); ++i) {
	if (lineComment) {
	    if (uncommented[i] == '\n') {
		lineComment = false;
	    } else {
		uncommented[i] = ' ';
	    }
	} else if (blockComment) {
	    if (uncommented[i] == '*' && i + 1 < uncommented.size () && uncommented[i + 1] == '/') {
		uncommented[i] = uncommented[i + 1] = ' ';
		++i;
		blockComment = false;
	    } else if (uncommented[i] != '\n') {
		uncommented[i] = ' ';
	    }
	} else if (uncommented[i] == '/' && i + 1 < uncommented.size () && uncommented[i + 1] == '/') {
	    uncommented[i] = uncommented[i + 1] = ' ';
	    ++i;
	    lineComment = true;
	} else if (uncommented[i] == '/' && i + 1 < uncommented.size () && uncommented[i + 1] == '*') {
	    uncommented[i] = uncommented[i + 1] = ' ';
	    ++i;
	    blockComment = true;
	}
    }

    const std::regex uniformDeclaration (
	R"(\buniform\s+(?:(?:lowp|mediump|highp|readonly|writeonly|coherent|volatile|restrict)\s+)*[A-Za-z_]\w*\s+([A-Za-z_]\w*))"
    );
    for (std::sregex_iterator it (uncommented.begin (), uncommented.end (), uniformDeclaration), end; it != end; ++it) {
	uniformNames.insert ((*it)[1].str ());
    }

    struct Replacement {
	std::size_t start;
	std::size_t length;
	std::string value;
    };
    std::vector<Replacement> replacements;
    const std::regex declarationPrefix (
	R"(^const\s+(?:(?:lowp|mediump|highp)\s+)?[A-Za-z_]\w*\s+([A-Za-z_]\w*)\s*=\s*)"
    );
    std::set<std::string> dynamicConstants;
    int braceDepth = 0;
    lineComment = false;
    blockComment = false;
    for (std::size_t i = 0; i < uncommented.size ();) {
	const char ch = uncommented[i];
	if (ch == '\n') {
	    lineComment = false;
	    ++i;
	    continue;
	}
	if (lineComment) {
	    ++i;
	    continue;
	}
	if (blockComment) {
	    if (ch == '*' && i + 1 < uncommented.size () && uncommented[i + 1] == '/') {
		blockComment = false;
		i += 2;
	    } else {
		++i;
	    }
	    continue;
	}
	if (ch == '/' && i + 1 < uncommented.size () && uncommented[i + 1] == '/') {
	    lineComment = true;
	    i += 2;
	    continue;
	}
	if (ch == '/' && i + 1 < uncommented.size () && uncommented[i + 1] == '*') {
	    blockComment = true;
	    i += 2;
	    continue;
	}
	if (ch == '{') {
	    ++braceDepth;
	    ++i;
	    continue;
	}
	if (ch == '}') {
	    braceDepth = std::max (0, braceDepth - 1);
	    ++i;
	    continue;
	}
	if (ch != 'c' || i + 5 > uncommented.size () || uncommented.compare (i, 5, "const") != 0
	    || (i > 0 && (std::isalnum (static_cast<unsigned char> (uncommented[i - 1])) || uncommented[i - 1] == '_'))
	    || (i + 5 < uncommented.size ()
		&& (std::isalnum (static_cast<unsigned char> (uncommented[i + 5])) || uncommented[i + 5] == '_'))) {
	    ++i;
	    continue;
	}

	const auto begin = uncommented.cbegin () + i;
	std::smatch prefix;
	const std::string remaining (begin, uncommented.cend ());
	if (!std::regex_search (remaining, prefix, declarationPrefix, std::regex_constants::match_continuous)) {
	    i += 5;
	    continue;
	}
	const std::string name = prefix[1].str ();
	const std::size_t expressionStart = i + static_cast<std::size_t> (prefix.length ());
	std::size_t statementEnd = expressionStart;
	int parenDepth = 0;
	int bracketDepth = 0;
	for (; statementEnd < uncommented.size (); ++statementEnd) {
	    const char exprChar = uncommented[statementEnd];
	    if (exprChar == '(') {
		++parenDepth;
	    } else if (exprChar == ')') {
		--parenDepth;
	    } else if (exprChar == '[') {
		++bracketDepth;
	    } else if (exprChar == ']') {
		--bracketDepth;
	    } else if (exprChar == ';' && parenDepth == 0 && bracketDepth == 0) {
		break;
	    }
	}
	if (statementEnd == uncommented.size ()) {
	    i += 5;
	    continue;
	}
	const std::string expression = uncommented.substr (expressionStart, statementEnd - expressionStart);
	const std::regex identifier (R"([A-Za-z_]\w*)");
	bool dynamic = false;
	for (std::sregex_iterator it (expression.begin (), expression.end (), identifier), end; it != end; ++it) {
	    if (uniformNames.contains ((*it)[0].str ()) || dynamicConstants.contains ((*it)[0].str ())) {
		dynamic = true;
		break;
	    }
	}
	if (!dynamic) {
	    i = statementEnd + 1;
	    continue;
	}

	const std::size_t sourceEnd = statementEnd + 1;
	dynamicConstants.insert (name);
	if (braceDepth == 0) {
	    replacements.push_back ({ i, sourceEnd - i, "#define " + name + " (" + expression + ")" });
	} else {
	    // Keep source line numbers stable for subsequent diagnostics.
	    replacements.push_back ({ i, 5, "     " });
	}
	adjusted.push_back (name);
	i = sourceEnd;
    }

    std::string result = source;
    for (auto it = replacements.rbegin (); it != replacements.rend (); ++it) {
	result.replace (it->start, it->length, it->value);
    }
    return result;
}

std::string computeShaderCacheKey (const std::string& vertex, const std::string& fragment) {
    constexpr uint64_t FNV_OFFSET_1 = 0xcbf29ce484222325ULL;
    constexpr uint64_t FNV_PRIME_1 = 0x100000001b3ULL;
    constexpr uint64_t FNV_OFFSET_2 = 0x84222325cbf29ce4ULL;
    constexpr uint64_t FNV_PRIME_2 = 0x100000001b3ULL;

    uint64_t h1 = FNV_OFFSET_1;
    uint64_t h2 = FNV_OFFSET_2;

    auto feed = [&] (const std::string_view sv) {
	for (const unsigned char c : sv) {
	    h1 = (h1 ^ c) * FNV_PRIME_1;
	    h2 = (h2 ^ c) * FNV_PRIME_2;
	}
    };

    feed ("v1\0");
    feed (vertex);
    feed ("\0\0");
    feed (fragment);

    char buf[33];
    std::snprintf (
	buf, sizeof (buf), "%016llx%016llx", static_cast<unsigned long long> (h1), static_cast<unsigned long long> (h2)
    );
    return std::string (buf, 32);
}

bool readCacheFile (const std::filesystem::path& path, std::string& out) {
    std::ifstream stream (path, std::ios::in | std::ios::binary);
    if (!stream.is_open ()) {
	return false;
    }
    std::ostringstream ss;
    ss << stream.rdbuf ();
    out = ss.str ();
    return !out.empty ();
}

void writeCacheFileAtomic (const std::filesystem::path& path, const std::string& content) {
    std::filesystem::path tmpPath = path;
    tmpPath += ".tmp." + std::to_string (getpid ());

    std::ofstream stream (tmpPath, std::ios::out | std::ios::binary | std::ios::trunc);
    if (!stream.is_open ()) {
	return;
    }
    stream.write (content.data (), static_cast<std::streamsize> (content.size ()));
    stream.close ();

    std::error_code ec;
    std::filesystem::rename (tmpPath, path, ec);
    if (ec) {
	std::filesystem::remove (tmpPath, ec);
    }
}
} // namespace

TBuiltInResource BuiltInResource = { .maxLights = 32,
				     .maxClipPlanes = 6,
				     .maxTextureUnits = 32,
				     .maxTextureCoords = 32,
				     .maxVertexAttribs = 64,
				     .maxVertexUniformComponents = 4096,
				     .maxVaryingFloats = 64,
				     .maxVertexTextureImageUnits = 32,
				     .maxCombinedTextureImageUnits = 80,
				     .maxTextureImageUnits = 32,
				     .maxFragmentUniformComponents = 4096,
				     .maxDrawBuffers = 32,
				     .maxVertexUniformVectors = 128,
				     .maxVaryingVectors = 8,
				     .maxFragmentUniformVectors = 16,
				     .maxVertexOutputVectors = 16,
				     .maxFragmentInputVectors = 15,
				     .minProgramTexelOffset = -8,
				     .maxProgramTexelOffset = 7,
				     .maxClipDistances = 8,
				     .maxComputeWorkGroupCountX = 65535,
				     .maxComputeWorkGroupCountY = 65535,
				     .maxComputeWorkGroupCountZ = 65535,
				     .maxComputeWorkGroupSizeX = 1024,
				     .maxComputeWorkGroupSizeY = 1024,
				     .maxComputeWorkGroupSizeZ = 64,
				     .maxComputeUniformComponents = 1024,
				     .maxComputeTextureImageUnits = 16,
				     .maxComputeImageUniforms = 8,
				     .maxComputeAtomicCounters = 8,
				     .maxComputeAtomicCounterBuffers = 1,
				     .maxVaryingComponents = 60,
				     .maxVertexOutputComponents = 64,
				     .maxGeometryInputComponents = 64,
				     .maxGeometryOutputComponents = 128,
				     .maxFragmentInputComponents = 128,
				     .maxImageUnits = 8,
				     .maxCombinedImageUnitsAndFragmentOutputs = 8,
				     .maxCombinedShaderOutputResources = 8,
				     .maxImageSamples = 0,
				     .maxVertexImageUniforms = 0,
				     .maxTessControlImageUniforms = 0,
				     .maxTessEvaluationImageUniforms = 0,
				     .maxGeometryImageUniforms = 0,
				     .maxFragmentImageUniforms = 8,
				     .maxCombinedImageUniforms = 8,
				     .maxGeometryTextureImageUnits = 16,
				     .maxGeometryOutputVertices = 256,
				     .maxGeometryTotalOutputComponents = 1024,
				     .maxGeometryUniformComponents = 1024,
				     .maxGeometryVaryingComponents = 64,
				     .maxTessControlInputComponents = 128,
				     .maxTessControlOutputComponents = 128,
				     .maxTessControlTextureImageUnits = 16,
				     .maxTessControlUniformComponents = 1024,
				     .maxTessControlTotalOutputComponents = 4096,
				     .maxTessEvaluationInputComponents = 128,
				     .maxTessEvaluationOutputComponents = 128,
				     .maxTessEvaluationTextureImageUnits = 16,
				     .maxTessEvaluationUniformComponents = 1024,
				     .maxTessPatchComponents = 120,
				     .maxPatchVertices = 32,
				     .maxTessGenLevel = 64,
				     .maxViewports = 16,
				     .maxVertexAtomicCounters = 0,
				     .maxTessControlAtomicCounters = 0,
				     .maxTessEvaluationAtomicCounters = 0,
				     .maxGeometryAtomicCounters = 0,
				     .maxFragmentAtomicCounters = 8,
				     .maxCombinedAtomicCounters = 8,
				     .maxAtomicCounterBindings = 1,
				     .maxVertexAtomicCounterBuffers = 0,
				     .maxTessControlAtomicCounterBuffers = 0,
				     .maxTessEvaluationAtomicCounterBuffers = 0,
				     .maxGeometryAtomicCounterBuffers = 0,
				     .maxFragmentAtomicCounterBuffers = 1,
				     .maxCombinedAtomicCounterBuffers = 1,
				     .maxAtomicCounterBufferSize = 16384,
				     .maxTransformFeedbackBuffers = 4,
				     .maxTransformFeedbackInterleavedComponents = 64,
				     .maxCullDistances = 8,
				     .maxCombinedClipAndCullDistances = 8,
				     .maxSamples = 4,
				     .maxMeshOutputVerticesNV = 256,
				     .maxMeshOutputPrimitivesNV = 512,
				     .maxMeshWorkGroupSizeX_NV = 32,
				     .maxMeshWorkGroupSizeY_NV = 1,
				     .maxMeshWorkGroupSizeZ_NV = 1,
				     .maxTaskWorkGroupSizeX_NV = 32,
				     .maxTaskWorkGroupSizeY_NV = 1,
				     .maxTaskWorkGroupSizeZ_NV = 1,
				     .maxMeshViewCountNV = 4,
				     .limits = {
					 .nonInductiveForLoops = true,
					 .whileLoops = true,
					 .doWhileLoops = true,
					 .generalUniformIndexing = true,
					 .generalAttributeMatrixVectorIndexing = true,
					 .generalVaryingIndexing = true,
					 .generalSamplerIndexing = true,
					 .generalVariableIndexing = true,
					 .generalConstantMatrixVectorIndexing = true,
				     } };

GLSLContext::GLSLContext () {
    assert (this->sInstance == nullptr);

    glslang::InitializeProcess ();

    const char* xdgCache = std::getenv ("XDG_CACHE_HOME");
    if (xdgCache != nullptr && *xdgCache != '\0') {
	this->m_cacheDir = std::filesystem::path (xdgCache) / "linux-wallpaperengine" / "shaders";
    } else {
	const char* home = std::getenv ("HOME");
	if (home != nullptr && *home != '\0') {
	    this->m_cacheDir = std::filesystem::path (home) / ".cache" / "linux-wallpaperengine" / "shaders";
	} else {
	    this->m_cacheDir = std::filesystem::temp_directory_path () / "linux-wallpaperengine-shaders";
	}
    }

    std::error_code ec;
    std::filesystem::create_directories (this->m_cacheDir, ec);
    if (ec) {
	sLog.error ("Failed to create shader cache directory: ", this->m_cacheDir.string (), " (", ec.message (), ")");
    }
}

GLSLContext::~GLSLContext () { glslang::FinalizeProcess (); }

GLSLContext& GLSLContext::get () {
    if (sInstance == nullptr) {
	sInstance = std::make_unique<GLSLContext> ();
    }

    return *sInstance;
}

std::pair<std::string, std::string> GLSLContext::toGlsl (const std::string& vertex, const std::string& fragment) {
    const std::string cacheKey = computeShaderCacheKey (vertex, fragment);

    {
	std::lock_guard<std::mutex> lock (this->m_cacheMutex);
	const auto it = this->m_memoryCache.find (cacheKey);
	if (it != this->m_memoryCache.end ()) {
	    return it->second;
	}
    }

    if (!this->m_cacheDir.empty ()) {
	const std::filesystem::path vertPath = this->m_cacheDir / (cacheKey + ".vert");
	const std::filesystem::path fragPath = this->m_cacheDir / (cacheKey + ".frag");

	std::string cachedVert;
	std::string cachedFrag;
	if (readCacheFile (vertPath, cachedVert) && readCacheFile (fragPath, cachedFrag)) {
	    auto cachedPair = std::make_pair (std::move (cachedVert), std::move (cachedFrag));
	    std::lock_guard<std::mutex> lock (this->m_cacheMutex);
	    this->m_memoryCache.emplace (cacheKey, cachedPair);
	    return cachedPair;
	}
    }
    std::vector<std::string> adjustedVaryings;
    std::string compatibleVaryings = normalizeXyOnlyVaryingMismatches (vertex, fragment, adjustedVaryings);
    std::vector<std::string> writableVaryings;
    const std::string writableFragment
	= normalizeWritableFragmentVaryings (compatibleVaryings, fragment, writableVaryings);
    std::vector<std::string> adjustedConstants;
    const std::string compatibleVertex = normalizeUniformDependentConstants (compatibleVaryings, adjustedConstants);
    const std::string compatibleFragment = normalizeUniformDependentConstants (writableFragment, adjustedConstants);
    std::vector<std::size_t> removedVertexEndifs;
    std::vector<std::size_t> removedFragmentEndifs;
    std::vector<std::string> adjustedVec2Operands;
    std::string validVertex = normalizeVec4OperandsInVec2Contexts (
	ensurePiFallback (neutralizeUnmatchedEndifs (compatibleVertex, removedVertexEndifs)), adjustedVec2Operands
    );
    std::string validFragment = normalizeVec4OperandsInVec2Contexts (
	ensurePiFallback (neutralizeUnmatchedEndifs (compatibleFragment, removedFragmentEndifs)), adjustedVec2Operands
    );
    std::vector<std::string> adjustedFloatConditions;
    validFragment = normalizeFloatConditions (validFragment, adjustedFloatConditions);
    validVertex = normalizePiMacroRedefinitions (validVertex);
    validFragment = normalizePiMacroRedefinitions (validFragment);

    std::string shaderName = "<unknown>";
    const std::regex shaderHeader (R"(// Processed shader ([^\r\n]+))");
    std::smatch shaderHeaderMatch;
    if (std::regex_search (fragment, shaderHeaderMatch, shaderHeader)) {
	shaderName = shaderHeaderMatch[1].str ();
    } else if (std::regex_search (vertex, shaderHeaderMatch, shaderHeader)) {
	shaderName = shaderHeaderMatch[1].str ();
    }
    for (const auto& name : adjustedVaryings) {
	sLog.out ("Narrowed XY-only GLSL varying to match fragment interface in ", shaderName, ": ", name);
    }
    for (const auto& name : writableVaryings) {
	sLog.out ("Copied writable fragment GLSL varying in ", shaderName, ": ", name);
    }
    for (const auto& name : adjustedConstants) {
	sLog.out ("Treating uniform-dependent GLSL const initializer as dynamic in ", shaderName, ": ", name);
    }
    std::sort (adjustedVec2Operands.begin (), adjustedVec2Operands.end ());
    adjustedVec2Operands.erase (
	std::unique (adjustedVec2Operands.begin (), adjustedVec2Operands.end ()), adjustedVec2Operands.end ()
    );
    for (const auto& name : adjustedVec2Operands) {
	sLog.out ("Using XY components of vec4 GLSL operand in a vec2 context in ", shaderName, ": ", name);
    }
    std::sort (adjustedFloatConditions.begin (), adjustedFloatConditions.end ());
    adjustedFloatConditions.erase (
	std::unique (adjustedFloatConditions.begin (), adjustedFloatConditions.end ()), adjustedFloatConditions.end ()
    );
    for (const auto& name : adjustedFloatConditions) {
	sLog.out ("Comparing scalar float GLSL condition to zero in ", shaderName, ": ", name);
    }
    for (const std::size_t line : removedVertexEndifs) {
	sLog.error ("Ignoring unmatched GLSL #endif in vertex shader ", shaderName, " at line ", line);
    }
    for (const std::size_t line : removedFragmentEndifs) {
	sLog.error ("Ignoring unmatched GLSL #endif in fragment shader ", shaderName, " at line ", line);
    }

    glslang::TShader vertexShader (EShLangVertex);

    const char* vertexSource = validVertex.c_str ();
    vertexShader.setStrings (&vertexSource, 1);
    vertexShader.setEntryPoint ("main");
    vertexShader.setEnvInput (glslang::EShSourceGlsl, EShLangVertex, glslang::EShClientOpenGL, 330);
    vertexShader.setEnvClient (glslang::EShClientOpenGL, glslang::EShTargetOpenGL_450);
    vertexShader.setEnvTarget (glslang::EShTargetSpv, glslang::EShTargetSpv_1_5);
    vertexShader.setAutoMapLocations (true);
    vertexShader.setAutoMapBindings (true);

    if (!vertexShader.parse (&BuiltInResource, 100, false, EShMsgDefault)) {
	sLog.error ("GLSL vertex unit parsing Failed in ", shaderName, ": ", vertexShader.getInfoLog ());
	return { "", "" };
    }
    const auto configureFragmentShader = [] (glslang::TShader& shader, const char* const* source) {
	shader.setStrings (source, 1);
	shader.setEntryPoint ("main");
	shader.setEnvInput (glslang::EShSourceGlsl, EShLangFragment, glslang::EShClientOpenGL, 330);
	shader.setEnvClient (glslang::EShClientOpenGL, glslang::EShTargetOpenGL_450);
	shader.setEnvTarget (glslang::EShTargetSpv, glslang::EShTargetSpv_1_5);
	shader.setAutoMapLocations (true);
	shader.setAutoMapBindings (true);
    };

    glslang::TShader fragmentShader (EShLangFragment);
    glslang::TShader fallbackFragmentShader (EShLangFragment);
    glslang::TShader* fragmentShaderForProgram = &fragmentShader;
    const char* fragmentSource = validFragment.c_str ();
    configureFragmentShader (fragmentShader, &fragmentSource);
    if (!fragmentShader.parse (&BuiltInResource, 100, false, EShMsgDefault)) {
	const std::string initialError = fragmentShader.getInfoLog ();
	std::vector<std::string> adjustedBlendOpacities;
	const std::string scalarizedFragment
	    = normalizeApplyBlendingVectorOpacity (validFragment, adjustedBlendOpacities);
	if (scalarizedFragment == validFragment || initialError.find ("ApplyBlending") == std::string::npos
	    || initialError.find ("no matching overloaded function") == std::string::npos) {
	    sLog.error ("GLSL fragment unit parsing Failed in ", shaderName, ": ", initialError);
	    return { "", "" };
	}
	validFragment = scalarizedFragment;
	for (const auto& name : adjustedBlendOpacities) {
	    sLog.out ("Using scalar component for GLSL ApplyBlending opacity in ", shaderName, ": ", name);
	}
	fragmentSource = validFragment.c_str ();
	configureFragmentShader (fallbackFragmentShader, &fragmentSource);
	if (!fallbackFragmentShader.parse (&BuiltInResource, 100, false, EShMsgDefault)) {
	    sLog.error (
		"GLSL fragment unit parsing Failed after ApplyBlending compatibility in ", shaderName, ": ",
		fallbackFragmentShader.getInfoLog ()
	    );
	    return { "", "" };
	}
	fragmentShaderForProgram = &fallbackFragmentShader;
    }
    glslang::TProgram program;
    program.addShader (&vertexShader);
    program.addShader (fragmentShaderForProgram);

    if (!program.link (EShMsgDefault)) {
	sLog.error ("Program Linking Failed in ", shaderName, ": ", program.getInfoLog ());
	return { "", "" };
    }

    std::vector<uint32_t> spirv;
    glslang::GlslangToSpv (*program.getIntermediate (EShLangVertex), spirv);

    spirv_cross::CompilerGLSL vertexCompiler (spirv);
    spirv_cross::CompilerGLSL::Options options;
    options.version = 330;
    options.es = false;
    vertexCompiler.set_common_options (options);

    spirv.clear ();
    glslang::GlslangToSpv (*program.getIntermediate (EShLangFragment), spirv);

    spirv_cross::CompilerGLSL fragmentCompiler (spirv);
    options.version = 330;
    options.es = false;
    fragmentCompiler.set_common_options (options);

    auto result = std::make_pair (
	vertexCompiler.compile () + "#if 0\n" + validVertex + "\n#endif",
	fragmentCompiler.compile () + "#if 0\n" + validFragment + "\n#endif"
    );

    if (!result.first.empty () && !result.second.empty ()) {
	if (!this->m_cacheDir.empty ()) {
	    const std::filesystem::path vertPath = this->m_cacheDir / (cacheKey + ".vert");
	    const std::filesystem::path fragPath = this->m_cacheDir / (cacheKey + ".frag");
	    writeCacheFileAtomic (vertPath, result.first);
	    writeCacheFileAtomic (fragPath, result.second);
	}

	std::lock_guard<std::mutex> lock (this->m_cacheMutex);
	this->m_memoryCache.emplace (cacheKey, result);
    }

    return result;
}

std::unique_ptr<GLSLContext> GLSLContext::sInstance = nullptr;

#ifndef __WP_LIB_MAIN_H__
#define __WP_LIB_MAIN_H__

// we don't really support windows, but doesn't hurt to have it here
#if (defined(_WIN32) || defined(__CYGWIN__)) && defined(WPENGINE_BUILD)
#ifdef WPENGINE_BUILD
#define WPENGINE_API __declspec (dllexport)
#else
#define WPENGINE_API __declspec (dllimport)
#endif
#elif defined(__GNUC__) && defined(WPENGINE_BUILD)
#define WPENGINE_API __attribute__ ((visibility ("default")))
#else
#define WPENGINE_API
#endif

#endif
/** @file ksym.h
 *
 * @brief Klish symbol
 */

#ifndef _klish_ksym_h
#define _klish_ksym_h

#include <klish/kcontext_base.h>

typedef struct ksym_s ksym_t;

// Callback function prototype
typedef int (*ksym_fn)(kcontext_t *context);

// Aliases for permanent flag
#define KSYM_USERDEFINED_PERMANENT TRI_UNDEFINED
#define KSYM_NONPERMANENT TRI_FALSE
#define KSYM_PERMANENT TRI_TRUE

// Aliases for sync flag
#define KSYM_USERDEFINED_SYNC TRI_UNDEFINED
#define KSYM_UNSYNC TRI_FALSE
#define KSYM_SYNC TRI_TRUE


C_DECL_BEGIN

// ksym_t
ksym_t *ksym_new(const char *name, ksym_fn function);
ksym_t *ksym_new_ext(const char *name, ksym_fn function,
	tri_t permanent, tri_t sync);
void ksym_free(ksym_t *sym);

const char *ksym_name(const ksym_t *sym);

ksym_fn ksym_function(const ksym_t *sym);
bool_t ksym_set_function(ksym_t *sym, ksym_fn fn);

tri_t ksym_permanent(const ksym_t *sym);
bool_t ksym_set_permanent(ksym_t *sym, tri_t permanent);

tri_t ksym_sync(const ksym_t *sym);
bool_t ksym_set_sync(ksym_t *sym, tri_t sync);

C_DECL_END

#endif // _klish_ksym_h

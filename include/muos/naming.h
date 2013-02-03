#ifndef __MUOS_NAMING_H__
#define __MUOS_NAMING_H__

#include <muos/decls.h>

BEGIN_DECLS

/**
 * Open a new channel that binds to the indicated path on the
 * filesystem.
 *
 * @return  if non-negative, the ID of a freshly opened channel
 *          belonging to the calling process, or the negated
 *          error code if negative.
 */
int NameAttach (char const full_path[]);

/**
 * Open a new connection to the channel bound at the indicated
 * filesystem path.
 *
 * @return  if non-negative, the ID of a freshly established
 *          connection, or the negated error code if negative.
 */
int NameOpen (char const full_path[]);

END_DECLS

#endif /* __MUOS_NAMING_H__ */

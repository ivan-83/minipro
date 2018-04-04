#ifndef __CONFIG_H_IN__
#define __CONFIG_H_IN__


/*--------------------------------------------------------------------*/
/* API, macros, includes. */

/* API */
#cmakedefine HAVE_MEMRCHR		1
#cmakedefine HAVE_MEMMEM		1
#cmakedefine HAVE_STRNCASECMP		1
#cmakedefine HAVE_REALLOCARRAY		1


/*--------------------------------------------------------------------*/
/* Package information. */
#define PACKAGE			@PROJECT_NAME@
#define VERSION			PACKAGE_VERSION
#define PACKAGE_NAME		"@PACKAGE_NAME@"
#define PACKAGE_VERSION		"@PACKAGE_VERSION@"
#define PACKAGE_URL		"@PACKAGE_URL@"
#define PACKAGE_BUGREPORT	"@PACKAGE_BUGREPORT@"
#define PACKAGE_STRING		"@PACKAGE_STRING@"
#define PACKAGE_DESCRIPTION	"@PACKAGE_DESCRIPTION@"


#define DB_FILE_DEF		"@SHARE_DIR@/minipro_db.ini"


#endif /* __CONFIG_H_IN__ */

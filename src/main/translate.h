/***************************************************************************
 translate.h - Handles multilanguage support
----------------------------------------------------------------------------
Began                : Sun Nov 17 2002
Copyright            : (C) 2002 by blight
Email                : blight@Ashitaka
****************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef __TRANSLATE_H__
#define __TRANSLATE_H__

#include "util.h"

#ifdef __cplusplus
extern "C" {
#endif

void        tr_init(void);                              // init multi-language support
void        tr_delete_languages(void);                  // free language list
list_t      tr_language_list(void);         // list of supported language name strings
int         tr_set_language(const char *name);      // set language to name
const char *tr(const char *text);           // translate text

#ifdef __cplusplus
}
#endif

#endif // __TRANSLATE_H__


/*
 * Copyright (c) 2026  Moddable Tech, Inc.
 *
 *   This file is part of the Moddable SDK Runtime.
 *
 *   The Moddable SDK Runtime is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU Lesser General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 *   The Moddable SDK Runtime is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU Lesser General Public License for more details.
 *
 *   You should have received a copy of the GNU Lesser General Public License
 *   along with the Moddable SDK Runtime.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

/*
	Replaces FreeType's ftsystem.c.

	Fonts are always opened from memory here -- FT_New_Memory_Face -- because a
	font is a resource in the application archive or a buffer in RAM, and most
	targets have no filesystem at all. FT_Stream_Open is the only piece of
	ftsystem.c anything references, and this is the stub that stands in for it.

	Memory comes from the SDK allocator instead of ftsystem.c's ft_smalloc. The
	font engine creates its library with FT_New_Library, passing its own
	FT_Memory, so it never calls FT_New_Memory -- but FT_Init_FreeType does, and
	it is defined here so that the library links whatever the toolchain decides
	to keep.
*/

#include <ft2build.h>
#include FT_CONFIG_CONFIG_H
#include <freetype/internal/ftstream.h>
#include <freetype/internal/ftobjs.h>
#include <freetype/internal/ftdebug.h>
#include "xsPlatform.h"
#include <freetype/fterrors.h>

static void *
ft_alloc( FT_Memory  memory,
          long       size )
{
	FT_UNUSED( memory );

	return c_malloc( (size_t)size );
}

static void
ft_free( FT_Memory  memory,
         void*      block )
{
	FT_UNUSED( memory );

	if ( block )
		c_free( block );
}

static void*
ft_realloc( FT_Memory  memory,
            long       cur_size,
            long       new_size,
            void*      block )
{
	FT_UNUSED( memory );
	FT_UNUSED( cur_size );

	return c_realloc( block, (size_t)new_size );
}

FT_BASE_DEF( FT_Memory )
FT_New_Memory( void )
{
	FT_Memory  memory = (FT_Memory)c_malloc( sizeof ( *memory ) );

	if ( memory )
	{
		memory->user = NULL;
		memory->alloc = ft_alloc;
		memory->free = ft_free;
		memory->realloc = ft_realloc;
	}

	return memory;
}

FT_BASE_DEF( void )
FT_Done_Memory( FT_Memory  memory )
{
	c_free( memory );
}

FT_BASE_DEF( FT_Error )
FT_Stream_Open( FT_Stream    stream,
                const char*  filepathname )
{
	FT_UNUSED( stream );
	FT_UNUSED( filepathname );

	return FT_THROW( Unimplemented_Feature );
}

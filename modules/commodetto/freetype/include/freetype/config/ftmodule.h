/*
 * This file registers the FreeType modules compiled into the library.
 *
 * MODDABLE: the upstream file lists every module FreeType ships. This one
 * registers only the modules the SDK builds, selected by the same
 * MODDEF_CFE_FT_* defines that choose the source files in the manifests --
 * see the Moddable section at the end of ftoption.h. Registering a module
 * whose sources are not compiled is a link error, so the two must agree.
 *
 * This file is included more than once per translation unit, with FT_USE_MODULE
 * defined differently each time, so it must not have an include guard. The
 * MODDEF_ macros it uses come from ftoption.h, which is included first.
 *
 */

#if MODDEF_CFE_FT_HINTINGAUTO
FT_USE_MODULE( FT_Module_Class, autofit_module_class )
#endif

FT_USE_MODULE( FT_Driver_ClassRec, tt_driver_class )

#if MODDEF_CFE_FT_CFF
FT_USE_MODULE( FT_Driver_ClassRec, cff_driver_class )
FT_USE_MODULE( FT_Module_Class, psaux_module_class )
FT_USE_MODULE( FT_Module_Class, psnames_module_class )
FT_USE_MODULE( FT_Module_Class, pshinter_module_class )
#endif

FT_USE_MODULE( FT_Module_Class, sfnt_module_class )
FT_USE_MODULE( FT_Renderer_Class, ft_smooth_renderer_class )

/* EOF */

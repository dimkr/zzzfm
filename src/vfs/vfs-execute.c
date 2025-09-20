/*
*  C Implementation: vfs-execute
*
* License: See COPYING file
*/

#include "vfs-execute.h"

#include <string.h>
#include <stdlib.h>

gboolean vfs_exec( const char* work_dir, char** argv, char** envp, const char* disp_name, GSpawnFlags flags, GError **err ) {
    return vfs_exec_on_screen( gdk_screen_get_default(), work_dir, argv, envp, disp_name, flags, err );
}

gboolean vfs_exec_on_screen( GdkScreen* screen, const char* work_dir, char** argv, char** envp,
                             const char* disp_name, GSpawnFlags flags, GError **err )
{
    gboolean ret;
    GSpawnChildSetupFunc setup_func = NULL;
    extern char **environ;
    char** new_env = envp;
    int i, n_env = 0;
    char* display_name;
    int display_index = -1, startup_id_index = -1;

    if ( ! envp )
        envp = environ;

    n_env = g_strv_length(envp);

    new_env = g_new0( char*, n_env + 4 );
    for ( i = 0; i < n_env; ++i )
    {
        /* g_debug( "old envp[%d] = \"%s\"" , i, envp[i]); */
#if GTK_CHECK_VERSION (3, 0, 0)
        if ( ( GDK_IS_X11_SCREEN ( screen ) && 0 == strncmp( envp[ i ], "DISPLAY=", 8 ) ) || ( ( ! GDK_IS_X11_SCREEN ( screen ) ) && ( 0 == strncmp( envp[ i ], "WAYLAND_DISPLAY=", 16 ) ) ) )
#else
        if ( 0 == strncmp( envp[ i ], "DISPLAY=", 8 ) )
#endif
            display_index = i;
        else
        {
            if ( 0 == strncmp( envp[ i ], "DESKTOP_STARTUP_ID=", 19 ) )
                startup_id_index = i;
            new_env[i] = g_strdup( envp[ i ] );
        }
    }

    //  This is taken from gdk_spawn_on_screen
    display_name = gdk_screen_make_display_name ( screen );

#if GTK_CHECK_VERSION (3, 0, 0)
    if ( ( ! GDK_IS_X11_SCREEN ( screen ) ) && display_index >= 0 )
        new_env[ display_index ] = g_strconcat( "WAYLAND_DISPLAY=", display_name, NULL );
    if ( ! GDK_IS_X11_SCREEN ( screen ) )
        new_env[ i++ ] = g_strconcat( "WAYLAND_DISPLAY=", display_name, NULL );
#else
    if ( FALSE ) do {} while ( 0 );
#endif
    else if ( display_index >= 0 )
        new_env[ display_index ] = g_strconcat( "DISPLAY=", display_name, NULL );
    else
        new_env[ i++ ] = g_strconcat( "DISPLAY=", display_name, NULL );

    g_free( display_name );
    new_env[ i ] = NULL;

    ret = g_spawn_async( work_dir, argv,  new_env, flags, NULL, NULL, NULL, err );

    /* for debugging */
#if 0
    g_debug( "debug vfs_execute_on_screen(): flags: %d, display_index=%d", flags, display_index );
    for( i = 0; argv[i]; ++i ) {
        g_debug( "argv[%d] = \"%s\"" , i, argv[i] );
    }
    for( i = 0; i < n_env /*new_env[i]*/; ++i ) {
        g_debug( "new_env[%d] = \"%s\"" , i, new_env[i] );
    }
    if( ret )
        g_debug( "the program was executed without error" );
    else
        g_debug( "launch failed: %s", (*err)->message );
#endif

    g_strfreev( new_env );
    return ret;
}

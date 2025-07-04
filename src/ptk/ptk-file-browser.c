#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <unistd.h>

#include <gtk/gtk.h>

#include <glib.h>
#include <glib/gi18n.h>

#include <gdk/gdkkeysyms.h>

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <malloc.h>

#include <fcntl.h>

#include <sys/stat.h>
#include <time.h>
#include <errno.h>

#include <fnmatch.h>

#include "ptk-file-browser.h"

#include "exo-icon-view.h"
#include "exo-tree-view.h"

#include "mime-type/mime-type.h"

#include "settings.h"
#include "ptk-app-chooser.h"

#include "ptk-file-icon-renderer.h"
#include "ptk-utils.h"
#include "ptk-input-dialog.h"
#include "ptk-file-task.h"
#include "ptk-file-misc.h"

#include "ptk-location-view.h"
#include "ptk-dir-tree-view.h"
#include "ptk-dir-tree.h"

#include "vfs-dir.h"
#include "vfs-utils.h"
#include "vfs-file-info.h"
#include "vfs-file-monitor.h"
#include "vfs-app-desktop.h"
#include "ptk-file-list.h"
#include "ptk-text-renderer.h"

#include "ptk-file-archiver.h"
#include "ptk-clipboard.h"

#include "ptk-file-menu.h"
#include "ptk-path-entry.h"
#include "find-files.h"
#include "main-window.h"

#include "gtk2-compat.h"

extern char* run_cmd;

static void ptk_file_browser_class_init( PtkFileBrowserClass* klass );
static void ptk_file_browser_init( PtkFileBrowser* file_browser );
static void ptk_file_browser_finalize( GObject *obj );
static void ptk_file_browser_get_property ( GObject *obj, guint prop_id, GValue *value, GParamSpec *pspec );
static void ptk_file_browser_set_property ( GObject *obj, guint prop_id, const GValue *value, GParamSpec *pspec );

/* Utility functions */
static GtkWidget* create_folder_view( PtkFileBrowser* file_browser, PtkFBViewMode view_mode );

static void init_list_view( PtkFileBrowser* file_browser, GtkTreeView* list_view );

static GtkWidget* ptk_file_browser_create_dir_tree( PtkFileBrowser* file_browser );

static void on_dir_file_listed( VFSDir* dir, gboolean is_cancelled, PtkFileBrowser* file_browser );

void ptk_file_browser_open_selected_files( PtkFileBrowser* file_browser );

static void   ptk_file_browser_cut_or_copy( PtkFileBrowser* file_browser, gboolean copy );

static void   ptk_file_browser_update_model( PtkFileBrowser* file_browser );

static gboolean   is_latin_shortcut_key( guint keyval );

/* Get GtkTreePath of the item at coordinate x, y */
static GtkTreePath*
folder_view_get_tree_path_at_pos( PtkFileBrowser* file_browser, int x, int y );

/* signal handlers */
static void   on_folder_view_item_activated ( ExoIconView *iconview, GtkTreePath *path, PtkFileBrowser* file_browser );
static void   on_folder_view_row_activated ( GtkTreeView *tree_view, GtkTreePath *path, GtkTreeViewColumn* col, PtkFileBrowser* file_browser );
static void   on_folder_view_item_sel_change ( ExoIconView *iconview, PtkFileBrowser* file_browser );

static gboolean   on_folder_view_button_press_event ( GtkWidget *widget, GdkEventButton *event, PtkFileBrowser* file_browser );
static gboolean   on_folder_view_button_release_event ( GtkWidget *widget, GdkEventButton *event, PtkFileBrowser* file_browser );
static gboolean   on_folder_view_popup_menu ( GtkWidget *widget, PtkFileBrowser* file_browser );

void on_dir_tree_row_activated ( GtkTreeView* view, GtkTreePath* path, GtkTreeViewColumn* column, PtkFileBrowser* file_browser );

/* Drag & Drop */
static void   on_folder_view_drag_data_received ( GtkWidget *widget,
                                    GdkDragContext *drag_context,
                                    gint x,
                                    gint y,
                                    GtkSelectionData *sel_data,
                                    guint info,
                                    guint time,
                                    gpointer user_data );

static void   on_folder_view_drag_data_get ( GtkWidget *widget,
                               GdkDragContext *drag_context,
                               GtkSelectionData *sel_data,
                               guint info,
                               guint time,
                               PtkFileBrowser *file_browser );

static void   on_folder_view_drag_begin ( GtkWidget *widget,
                            GdkDragContext *drag_context,
                            PtkFileBrowser* file_browser );

static gboolean   on_folder_view_drag_motion ( GtkWidget *widget,
                             GdkDragContext *drag_context,
                             gint x,
                             gint y,
                             guint time,
                             PtkFileBrowser* file_browser );

static gboolean   on_folder_view_drag_leave ( GtkWidget *widget,
                            GdkDragContext *drag_context,
                            guint time,
                            PtkFileBrowser* file_browser );

static gboolean   on_folder_view_drag_drop ( GtkWidget *widget,
                           GdkDragContext *drag_context,
                           gint x,
                           gint y,
                           guint time,
                           PtkFileBrowser* file_browser );

static void   on_folder_view_drag_end ( GtkWidget *widget,  GdkDragContext *drag_context,  PtkFileBrowser* file_browser );

/* Default signal handlers */
static void ptk_file_browser_before_chdir( PtkFileBrowser* file_browser, const char* path, gboolean* cancel );
static void ptk_file_browser_after_chdir( PtkFileBrowser* file_browser );
static void ptk_file_browser_content_change( PtkFileBrowser* file_browser );
static void ptk_file_browser_sel_change( PtkFileBrowser* file_browser );
static void ptk_file_browser_open_item( PtkFileBrowser* file_browser, const char* path, int action );
static void ptk_file_browser_pane_mode_change( PtkFileBrowser* file_browser );
void ptk_file_browser_update_views( GtkWidget* item, PtkFileBrowser* file_browser );
void focus_folder_view( PtkFileBrowser* file_browser );
void on_shortcut_new_tab_here( GtkMenuItem* item, PtkFileBrowser* file_browser );
void enable_toolbar( PtkFileBrowser* file_browser );
void show_thumbnails( PtkFileBrowser* file_browser, PtkFileList* list, gboolean is_big, int max_file_size );

static int   file_list_order_from_sort_order( PtkFBSortOrder order );

static GtkPanedClass *parent_class = NULL;

enum {
    BEFORE_CHDIR_SIGNAL,
    BEGIN_CHDIR_SIGNAL,
    AFTER_CHDIR_SIGNAL,
    OPEN_ITEM_SIGNAL,
    CONTENT_CHANGE_SIGNAL,
    SEL_CHANGE_SIGNAL,
    PANE_MODE_CHANGE_SIGNAL,
    N_SIGNALS
};

enum{
    RESPONSE_RUN = 100,
    RESPONSE_RUNTERMINAL = 101,
};

static void enter_callback( GtkEntry* entry, GtkDialog* dlg );

static char *replace_str(char *str, char *orig, char *rep);

static void rebuild_toolbox( GtkWidget* widget, PtkFileBrowser* file_browser );
static void rebuild_side_toolbox( GtkWidget* widget, PtkFileBrowser* file_browser );

static guint signals[ N_SIGNALS ] = { 0 };

static guint folder_view_auto_scroll_timer = 0;
static GtkDirectionType folder_view_auto_scroll_direction = 0;

/*  Drag & Drop/Clipboard targets  */
static GtkTargetEntry drag_targets[] = {
                                           {"text/uri-list", 0 , 0 }
                                       };

#define GDK_ACTION_ALL  (GDK_ACTION_MOVE|GDK_ACTION_COPY|GDK_ACTION_LINK)

// must match main-window.c  main_window_socket_command
const char* column_titles[] =
{
    N_( "Name" ), N_( "Size" ), N_( "Type" ), N_( "Permission" ), N_( "Owner" ), N_( "Modified" ), N_( "Created" )
};

const char* column_names[] =
{
    "detcol_name", "detcol_size", "detcol_type", "detcol_perm", "detcol_owner", "detcol_date",
};               // howdy bub



GType ptk_file_browser_get_type() {
    static GType type = G_TYPE_INVALID;
    if ( G_UNLIKELY ( type == G_TYPE_INVALID ) )
    {
        static const GTypeInfo info =
            {
                sizeof ( PtkFileBrowserClass ), NULL, NULL,
                ( GClassInitFunc ) ptk_file_browser_class_init, NULL, NULL, sizeof ( PtkFileBrowser ), 0,
                ( GInstanceInitFunc ) ptk_file_browser_init, NULL,
            };
        //type = g_type_register_static ( GTK_TYPE_HPANED, "PtkFileBrowser", &info, 0 );
        type = g_type_register_static ( GTK_TYPE_VBOX, "PtkFileBrowser", &info, 0 );
    }
    return type;
}


/* These g_cclosure_marshal functions are from gtkmarshal.c, the deprecated
 * functions renamed from gtk_* to g_cclosure_*, to match the naming convention
 * of the non-deprecated glib functions.   Added for gtk3 port. */
static void g_cclosure_marshal_VOID__POINTER_POINTER(
                                GClosure     *closure,
                                GValue       *return_value G_GNUC_UNUSED,
                                guint         n_param_values,
                                const GValue *param_values,
                                gpointer      invocation_hint G_GNUC_UNUSED,
                                gpointer      marshal_data)
{
    register GCClosure *cc = (GCClosure *) closure;
    register gpointer data1, data2;

    g_return_if_fail (n_param_values == 3);

    if (G_CCLOSURE_SWAP_DATA (closure))
    {
        data1 = closure->data;
        data2 = g_value_peek_pointer (param_values + 0);
    } else {
        data1 = g_value_peek_pointer (param_values + 0);
        data2 = closure->data;
    }

    typedef void (*GMarshalFunc_VOID__POINTER_POINTER) (gpointer data1,
                                                        gpointer arg_1,
                                                        gpointer arg_2,
                                                        gpointer data2);

    register GMarshalFunc_VOID__POINTER_POINTER callback =
                        (GMarshalFunc_VOID__POINTER_POINTER) (marshal_data ?
                                                marshal_data : cc->callback);

    callback (data1,
              g_value_get_pointer (param_values + 1),
              g_value_get_pointer (param_values + 2),
              data2);
}

static void   g_cclosure_marshal_VOID__POINTER_INT (GClosure     *closure,
                                      GValue       *return_value,
                                      guint         n_param_values,
                                      const GValue *param_values,
                                      gpointer      invocation_hint,
                                      gpointer      marshal_data)
{
  typedef void (*GMarshalFunc_VOID__POINTER_INT) (gpointer     data1,
                                                  gpointer     arg_1,
                                                  gint         arg_2,
                                                  gpointer     data2);
  register GMarshalFunc_VOID__POINTER_INT callback;
  register GCClosure *cc = (GCClosure*) closure;
  register gpointer data1, data2;

  g_return_if_fail (n_param_values == 3);

  if (G_CCLOSURE_SWAP_DATA (closure))
    {
      data1 = closure->data;
      data2 = g_value_peek_pointer (param_values + 0);
    } else {
      data1 = g_value_peek_pointer (param_values + 0);
      data2 = closure->data;
    }
  callback = (GMarshalFunc_VOID__POINTER_INT) (marshal_data ?  marshal_data : cc->callback);

  callback (data1, g_value_get_pointer (param_values + 1), g_value_get_int (param_values + 2), data2);
}


void ptk_file_browser_class_init( PtkFileBrowserClass* klass ) {
    GObjectClass * object_class;
    GtkWidgetClass *widget_class;

    object_class = ( GObjectClass * ) klass;
    parent_class = g_type_class_peek_parent ( klass );

    object_class->set_property = ptk_file_browser_set_property;
    object_class->get_property = ptk_file_browser_get_property;
    object_class->finalize = ptk_file_browser_finalize;

    widget_class = GTK_WIDGET_CLASS ( klass );

    /* Signals */

    klass->before_chdir = ptk_file_browser_before_chdir;
    klass->after_chdir = ptk_file_browser_after_chdir;
    klass->open_item = ptk_file_browser_open_item;
    klass->content_change = ptk_file_browser_content_change;
    klass->sel_change = ptk_file_browser_sel_change;
    klass->pane_mode_change = ptk_file_browser_pane_mode_change;

    /* before-chdir is emitted when PtkFileBrowser is about to change
    * its working directory. The 1st param is the path of the dir (in UTF-8),
    * and the 2nd param is a gboolean*, which can be filled by the
    * signal handler with TRUE to cancel the operation.
    */
    signals[ BEFORE_CHDIR_SIGNAL ] =
        g_signal_new ( "before-chdir",
                       G_TYPE_FROM_CLASS ( klass ),
                       G_SIGNAL_RUN_LAST,
                       G_STRUCT_OFFSET ( PtkFileBrowserClass, before_chdir ),
                       NULL, NULL,
                       g_cclosure_marshal_VOID__POINTER_POINTER,
                       G_TYPE_NONE, 2, G_TYPE_POINTER, G_TYPE_POINTER );

    signals[ BEGIN_CHDIR_SIGNAL ] =
        g_signal_new ( "begin-chdir",
                       G_TYPE_FROM_CLASS ( klass ),
                       G_SIGNAL_RUN_LAST,
                       G_STRUCT_OFFSET ( PtkFileBrowserClass, begin_chdir ),
                       NULL, NULL,
                       g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0 );

    signals[ AFTER_CHDIR_SIGNAL ] =
        g_signal_new ( "after-chdir",
                       G_TYPE_FROM_CLASS ( klass ),
                       G_SIGNAL_RUN_LAST,
                       G_STRUCT_OFFSET ( PtkFileBrowserClass, after_chdir ),
                       NULL, NULL,
                       g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0 );

    /*
    * This signal is sent when a directory is about to be opened
    * arg1 is the path to be opened, and arg2 is the type of action,
    * ex: open in tab, open in terminal...etc.
    */
    signals[ OPEN_ITEM_SIGNAL ] =
        g_signal_new ( "open-item",
                       G_TYPE_FROM_CLASS ( klass ),
                       G_SIGNAL_RUN_LAST,
                       G_STRUCT_OFFSET ( PtkFileBrowserClass, open_item ),
                       NULL, NULL,
                       g_cclosure_marshal_VOID__POINTER_INT, G_TYPE_NONE, 2,
                       G_TYPE_POINTER, G_TYPE_INT );

    signals[ CONTENT_CHANGE_SIGNAL ] =
        g_signal_new ( "content-change",
                       G_TYPE_FROM_CLASS ( klass ),
                       G_SIGNAL_RUN_LAST,
                       G_STRUCT_OFFSET ( PtkFileBrowserClass, content_change ),
                       NULL, NULL,
                       g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0 );

    signals[ SEL_CHANGE_SIGNAL ] =
        g_signal_new ( "sel-change",
                       G_TYPE_FROM_CLASS ( klass ),
                       G_SIGNAL_RUN_LAST,
                       G_STRUCT_OFFSET ( PtkFileBrowserClass, sel_change ),
                       NULL, NULL,
                       g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0 );

    signals[ PANE_MODE_CHANGE_SIGNAL ] =
        g_signal_new ( "pane-mode-change",
                       G_TYPE_FROM_CLASS ( klass ),
                       G_SIGNAL_RUN_LAST,
                       G_STRUCT_OFFSET ( PtkFileBrowserClass, pane_mode_change ),
                       NULL, NULL,
                       g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0 );

}


gboolean ptk_file_browser_slider_release( GtkWidget *widget, GdkEventButton *event, PtkFileBrowser* file_browser ) {
    int pos;
    FMMainWindow* main_window = (FMMainWindow*)file_browser->main_window;
    int p = file_browser->mypanel;
    char mode = main_window->panel_context[p-1];

    XSet* set = xset_get_panel_mode( p, "slider_positions", mode );

    if ( widget == file_browser->hpane )
    {
        pos = gtk_paned_get_position( GTK_PANED( file_browser->hpane ) );
        if ( !main_window->fullscreen )
        {
            g_free( set->x );
            set->x = g_strdup_printf( "%d", pos );
        }
        main_window->panel_slide_x[p-1] = pos;
//printf("    slide_x = %d\n", pos );
    } else {
//printf("ptk_file_browser_slider_release fb=%#x  (panel %d)  mode = %d\n", file_browser, p, mode );
        pos = gtk_paned_get_position( GTK_PANED( file_browser->side_vpane_top ) );
        if ( !main_window->fullscreen )
        {
            g_free( set->y );
            set->y = g_strdup_printf( "%d", pos );
        }
        main_window->panel_slide_y[p-1] = pos;
//printf("    slide_y = %d  ", pos );

        pos = gtk_paned_get_position( GTK_PANED( file_browser->side_vpane_bottom ) );
        if ( !main_window->fullscreen )
        {
            g_free( set->s );
            set->s = g_strdup_printf( "%d", pos );
        }
        main_window->panel_slide_s[p-1] = pos;
//printf("slide_s = %d\n", pos );
    }
    return FALSE;
}


void ptk_file_browser_select_file( PtkFileBrowser* file_browser, const char* path ) {
    GtkTreeIter it;
    GtkTreePath* tree_path;
    GtkTreeSelection* tree_sel;
    GtkTreeModel* model = NULL;
    VFSFileInfo* file;
    const char* file_name;

    PtkFileList* list = PTK_FILE_LIST( file_browser->file_list );
    if ( file_browser->view_mode == PTK_FB_COMPACT_VIEW )
    {
        exo_icon_view_unselect_all( EXO_ICON_VIEW( file_browser->folder_view ) );    //   howdy  compactview still requires exoiconview
        model = exo_icon_view_get_model( EXO_ICON_VIEW( file_browser->folder_view ) );
    }
    else if ( file_browser->view_mode == PTK_FB_LIST_VIEW )
    {
        model = gtk_tree_view_get_model( GTK_TREE_VIEW( file_browser->folder_view ) );
        tree_sel = gtk_tree_view_get_selection( GTK_TREE_VIEW( file_browser->folder_view ) );
        gtk_tree_selection_unselect_all( tree_sel );
    }
    if ( !model )
        return;
    char* name = g_path_get_basename( path );

    if ( gtk_tree_model_get_iter_first( model, &it ) )
    {
        do
        {
            gtk_tree_model_get( model, &it, COL_FILE_INFO, &file, -1 );
            if ( file )
            {
                file_name = vfs_file_info_get_name( file );
                if ( !strcmp( file_name, name ) )
                {
                    tree_path = gtk_tree_model_get_path( GTK_TREE_MODEL(list), &it );
                    if ( file_browser->view_mode == PTK_FB_COMPACT_VIEW )
                    {
                        exo_icon_view_select_path( EXO_ICON_VIEW( file_browser->folder_view ), tree_path );
                        exo_icon_view_set_cursor( EXO_ICON_VIEW( file_browser->folder_view ), tree_path, NULL, FALSE );
                        exo_icon_view_scroll_to_path( EXO_ICON_VIEW( file_browser->folder_view ), tree_path, TRUE, .25, 0 );
                    }
                    else if ( file_browser->view_mode == PTK_FB_LIST_VIEW )
                    {
                        gtk_tree_selection_select_path( tree_sel, tree_path );
                        gtk_tree_view_set_cursor( GTK_TREE_VIEW( file_browser->folder_view ), tree_path, NULL, FALSE);
                        gtk_tree_view_scroll_to_cell( GTK_TREE_VIEW( file_browser->folder_view ), tree_path, NULL, TRUE, .25, 0 );
                    }
                    gtk_tree_path_free( tree_path );
                    vfs_file_info_unref( file );
                    break;
                }
                vfs_file_info_unref( file );
            }
        }
        while ( gtk_tree_model_iter_next( model, &it ) );
    }
    g_free( name );
}


void save_command_history( GtkEntry* entry ) {
    GList* l;

    EntryData* edata = (EntryData*)g_object_get_data( G_OBJECT( entry ), "edata" );
    if ( !edata )
        return;
    const char* text = gtk_entry_get_text( GTK_ENTRY( entry ) );
    // remove duplicates
    while ( l = g_list_find_custom( xset_cmd_history, text, (GCompareFunc)g_strcmp0 ) )
    {
        g_free( (char*)l->data );
        xset_cmd_history = g_list_delete_link( xset_cmd_history, l );
    }
    xset_cmd_history = g_list_prepend( xset_cmd_history, g_strdup( text ) );
    // shorten to 200 entries
    while ( g_list_length( xset_cmd_history ) > 200 )
    {
        l = g_list_last( xset_cmd_history );
        g_free( (char*)l->data );
        xset_cmd_history = g_list_delete_link( xset_cmd_history, l );
    }
}


gboolean on_address_bar_focus_in( GtkWidget *entry, GdkEventFocus* evt, PtkFileBrowser* file_browser ) {
    ptk_file_browser_focus_me( file_browser );
    return FALSE;
}


void on_address_bar_activate( GtkWidget* entry, PtkFileBrowser* file_browser ) {
    const char* text;
    gchar *dir_path, *final_path;
    GList* l;
    char* str;
    struct stat64 statbuf;

    text = gtk_entry_get_text( GTK_ENTRY( entry ) );

    gtk_editable_select_region( (GtkEditable*)entry, 0, 0 );    // clear selection

    // Convert to on-disk encoding
    dir_path = g_filename_from_utf8( text, -1, NULL, NULL, NULL );
    final_path = vfs_file_resolve_path( ptk_file_browser_get_cwd( file_browser ), dir_path );
    g_free( dir_path );

    if ( text[0] == '\0' )
    {
        g_free( final_path );
        return;
    }

    gboolean final_path_exists = g_file_test( final_path, G_FILE_TEST_EXISTS );

    if ( !final_path_exists &&  ( text[0] == '$' || text[0] == '+' || text[0] == '&' || text[0] == '!' || text[0] == '\0' ) )
    {
        // command
        char* command;
        char* trim_command;
        gboolean as_root = FALSE;
        gboolean in_terminal = FALSE;
        gboolean as_task = TRUE;
        char* prefix = g_strdup( "" );
        while ( text[0] == '$' || text[0] == '+' || text[0] == '&' ||  text[0] == '!' )
        {
            if ( text[0] == '+' )
                in_terminal = TRUE;
            else if ( text[0] == '&' )
                as_task = FALSE;
            else if ( text[0] == '!' )
                as_root = TRUE;

            str = prefix;
            prefix = g_strdup_printf( "%s%c", str, text[0] );
            g_free( str );
            text++;
        }
        gboolean is_space = text[0] == ' ';
        command = g_strdup( text );
        trim_command = g_strstrip( command );
        if ( trim_command[0] == '\0' )
        {
            g_free( command );
            g_free( prefix );
            ptk_path_entry_help( entry, GTK_WIDGET( file_browser ) );
            gtk_editable_set_position( GTK_EDITABLE( entry ), -1 );
            return;
        }

        save_command_history( GTK_ENTRY( entry ) );

        // task
        char* task_name = g_strdup( gtk_entry_get_text( GTK_ENTRY( entry ) ) );
        const char* cwd = ptk_file_browser_get_cwd( file_browser );
        PtkFileTask* task = ptk_file_exec_new( task_name, cwd, GTK_WIDGET( file_browser ), file_browser->task_view );
        g_free( task_name );
        // don't free cwd!
        task->task->exec_browser = file_browser;
        task->task->exec_command = replace_line_subs( trim_command );
        g_free( command );
        if ( as_root )
            task->task->exec_as_user = g_strdup_printf( "root" );
        if ( !as_task )
            task->task->exec_sync = FALSE;
        else
            task->task->exec_sync = !in_terminal;
        task->task->exec_show_output = TRUE;
        task->task->exec_show_error = TRUE;
        task->task->exec_export = TRUE;
        task->task->exec_terminal = in_terminal;
        task->task->exec_keep_terminal = as_task;
        //task->task->exec_keep_tmp = TRUE;
        ptk_file_task_run( task );
        //gtk_widget_grab_focus( GTK_WIDGET( file_browser->folder_view ) );

        // reset entry text
        str = prefix;
        prefix = g_strdup_printf( "%s%s", str, is_space ? " " : "" );
        g_free( str );
        gtk_entry_set_text( GTK_ENTRY( entry ), prefix );
        g_free( prefix );
        gtk_editable_set_position( GTK_EDITABLE( entry ), -1 );
    }
    else if ( !final_path_exists && text[0] == '%' )
    {
        str = g_strdup( ++text );
        g_strstrip( str );
        if ( str && str[0] != '\0' )
        {
            save_command_history( GTK_ENTRY( entry ) );
            ptk_file_browser_select_pattern( NULL, file_browser, str );
        }
        g_free( str );
    }
    else if ( ( text[0] != '/' && strstr( text, ":/" ) ) ||   g_str_has_prefix( text, "//" ) )
    {
        save_command_history( GTK_ENTRY( entry ) );
        str = g_strdup( text );
        ptk_location_view_mount_network( file_browser, str, FALSE, FALSE );
        g_free( str );
        return;
    } else {
        // path?
        // clean double slashes
        while ( strstr( final_path, "//" ) )
        {
            str = final_path;
            final_path = replace_string( str, "//", "/", FALSE );
            g_free( str );
        }
        if ( g_file_test( final_path, G_FILE_TEST_IS_DIR ) )
        {
            // open dir
            if ( strcmp( final_path, ptk_file_browser_get_cwd( file_browser ) ) )
                ptk_file_browser_chdir( file_browser, final_path, PTK_FB_CHDIR_ADD_HISTORY );
            gtk_widget_grab_focus( GTK_WIDGET( file_browser->folder_view ) );
        }
        else if ( final_path_exists )
        {
            if ( stat64( final_path, &statbuf ) == 0 &&  S_ISBLK( statbuf.st_mode ) &&
                            ptk_location_view_open_block( final_path, FALSE ) )
            {
                // ptk_location_view_open_block opened device
            } else {
                // open dir and select file
                dir_path = g_path_get_dirname( final_path );
                if ( strcmp( dir_path, ptk_file_browser_get_cwd( file_browser ) ) )
                {
                    g_free( file_browser->select_path );
                    file_browser->select_path = strdup( final_path );
                    ptk_file_browser_chdir( file_browser, dir_path, PTK_FB_CHDIR_ADD_HISTORY );
                }
                else
                    ptk_file_browser_select_file( file_browser, final_path );
                g_free( dir_path );
            }
            gtk_widget_grab_focus( GTK_WIDGET( file_browser->folder_view ) );
        }
        gtk_editable_set_position( GTK_EDITABLE( entry ), -1 );

        // inhibit auto seek because if multiple completions will change dir
        EntryData* edata = (EntryData*)g_object_get_data( G_OBJECT( entry ), "edata" );
        if ( edata && edata->seek_timer )
        {
            g_source_remove( edata->seek_timer );
            edata->seek_timer = 0;
        }
    }
    g_free( final_path );
}


void ptk_file_browser_add_toolbar_widget( gpointer set_ptr, GtkWidget* widget ) {   // store the toolbar widget created by set for later change of status
    char x;
    XSet* set = (XSet*)set_ptr;

    if ( !( set && !set->lock && set->browser && set->tool &&  GTK_IS_WIDGET( widget ) ) )
        return;

    if ( set->tool == XSET_TOOL_UP )
        x = 0;
    else if ( set->tool == XSET_TOOL_BACK || set->tool == XSET_TOOL_BACK_MENU )
        x = 1;
    else if ( set->tool == XSET_TOOL_FWD || set->tool == XSET_TOOL_FWD_MENU )
        x = 2;
    else if ( set->tool == XSET_TOOL_DEVICES )
        x = 3;
    else if ( set->tool == XSET_TOOL_BOOKMARKS )
        x = 4;
    else if ( set->tool == XSET_TOOL_TREE )
        x = 5;
    else if ( set->tool == XSET_TOOL_SHOW_HIDDEN )
        x = 6;
    else if ( set->tool == XSET_TOOL_CUSTOM &&  set->menu_style == XSET_MENU_CHECK )
    {
        x = 7;
        // attach set pointer to custom checkboxes so we can find it
        g_object_set_data( G_OBJECT( widget ), "set", set );
    }
    else if ( set->tool == XSET_TOOL_SHOW_THUMB )
        x = 8;
    else if ( set->tool == XSET_TOOL_LARGE_ICONS )
        x = 9;
    else
        return;

    set->browser->toolbar_widgets[x] = g_slist_append( set->browser->toolbar_widgets[x], widget );
}


void ptk_file_browser_update_toolbar_widgets( PtkFileBrowser* file_browser, gpointer set_ptr, char tool_type ) {
    char x;
    GSList* l;
    GtkWidget* widget;
    XSet* set = (XSet*)set_ptr;

    if ( !PTK_IS_FILE_BROWSER( file_browser ) )
        return;

    if ( set && !set->lock && set->menu_style == XSET_MENU_CHECK &&
                              set->tool == XSET_TOOL_CUSTOM )
    {
        // a custom checkbox is being updated
        for ( l = file_browser->toolbar_widgets[7]; l; l = l->next )
        {
            if ( (XSet*)g_object_get_data( G_OBJECT( l->data ), "set" ) == set )
            {
                widget = GTK_WIDGET( l->data );
                if ( GTK_IS_TOGGLE_BUTTON( widget ) )
                {
                    gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( widget ), set->b == XSET_B_TRUE );
                    return;
                }
            }
        }
        g_warning( "ptk_file_browser_update_toolbar_widget widget not found for set" );
        return;
    }
    else if ( set_ptr )
    {
        g_warning( "ptk_file_browser_update_toolbar_widget invalid set_ptr or set" );
        return;
    }

    // builtin tool
    gboolean b;
    if ( tool_type == XSET_TOOL_UP )
    {
        x = 0;
        const char* cwd = ptk_file_browser_get_cwd( file_browser );
        b = !cwd || ( cwd && strcmp( cwd, "/" ) );
    }
    else if ( tool_type == XSET_TOOL_BACK || tool_type == XSET_TOOL_BACK_MENU )
    {
        x = 1;
        b = file_browser->curHistory && file_browser->curHistory->prev;
    }
    else if ( tool_type == XSET_TOOL_FWD || tool_type == XSET_TOOL_FWD_MENU )
    {
        x = 2;
        b = file_browser->curHistory && file_browser->curHistory->next;
    }
    else if ( tool_type == XSET_TOOL_DEVICES )
    {
        x = 3;
        b = !!file_browser->side_dev;
    }
    else if ( tool_type == XSET_TOOL_BOOKMARKS )
    {
        x = 4;
        b = !!file_browser->side_book;
    }
    else if ( tool_type == XSET_TOOL_TREE )
    {
        x = 5;
        b = !!file_browser->side_dir;
    }
    else if ( tool_type == XSET_TOOL_SHOW_HIDDEN )
    {
        x = 6;
        b = file_browser->show_hidden_files;
    }
    else if ( tool_type == XSET_TOOL_SHOW_THUMB )
    {
        x = 8;
        b = app_settings.show_thumbnail;
    }
    else if ( tool_type == XSET_TOOL_LARGE_ICONS )
    {
        x = 9;
        b = file_browser->large_icons;
    } else {
        g_warning( "ptk_file_browser_update_toolbar_widget invalid tool_type" );
        return;
    }

    // update all widgets in list
    for ( l = file_browser->toolbar_widgets[x]; l; l = l->next )
    {
        widget = GTK_WIDGET( l->data );
        if ( GTK_IS_TOGGLE_BUTTON( widget ) )
            gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( widget ), b );
        else if ( GTK_IS_WIDGET( widget ) )
            gtk_widget_set_sensitive( widget, b );
        else
        {
            g_warning( "ptk_file_browser_update_toolbar_widget invalid widget" );
        }
    }
}


void enable_toolbar( PtkFileBrowser* file_browser ) {
    ptk_file_browser_update_toolbar_widgets( file_browser, NULL, XSET_TOOL_BACK );
    ptk_file_browser_update_toolbar_widgets( file_browser, NULL, XSET_TOOL_FWD );
    ptk_file_browser_update_toolbar_widgets( file_browser, NULL, XSET_TOOL_UP );
    ptk_file_browser_update_toolbar_widgets( file_browser, NULL, XSET_TOOL_DEVICES );
    ptk_file_browser_update_toolbar_widgets( file_browser, NULL, XSET_TOOL_BOOKMARKS );
    ptk_file_browser_update_toolbar_widgets( file_browser, NULL, XSET_TOOL_TREE );
    ptk_file_browser_update_toolbar_widgets( file_browser, NULL, XSET_TOOL_SHOW_HIDDEN );
    ptk_file_browser_update_toolbar_widgets( file_browser, NULL, XSET_TOOL_SHOW_THUMB );
    ptk_file_browser_update_toolbar_widgets( file_browser, NULL, XSET_TOOL_LARGE_ICONS );
}


static void rebuild_toolbox( GtkWidget* widget, PtkFileBrowser* file_browser ) {
//printf(" rebuild_toolbox\n");
    if ( !file_browser )
        return;

    FMMainWindow* main_window = (FMMainWindow*)file_browser->main_window;
    int p = file_browser->mypanel;
    char mode = main_window ? main_window->panel_context[p-1] : 0;

    gboolean show_tooltips = !xset_get_b_panel( 1, "tool_l" );

    // destroy
    if ( file_browser->toolbar )
    {
        if ( GTK_IS_WIDGET( file_browser->toolbar ) )
            gtk_widget_destroy( file_browser->toolbar );
        file_browser->toolbar = NULL;
        file_browser->path_bar = NULL;
    }


    if ( !file_browser->path_bar )
    {
        file_browser->path_bar = ptk_path_entry_new( file_browser );
        g_signal_connect( file_browser->path_bar, "activate", G_CALLBACK(on_address_bar_activate), file_browser );
        g_signal_connect( file_browser->path_bar, "focus-in-event", G_CALLBACK(on_address_bar_focus_in), file_browser );
    }

    // create toolbar
    file_browser->toolbar = gtk_toolbar_new();
    gtk_box_pack_start( GTK_BOX( file_browser->toolbox ), file_browser->toolbar, TRUE, TRUE, 0 );
    gtk_toolbar_set_style( GTK_TOOLBAR( file_browser->toolbar ), GTK_TOOLBAR_ICONS );
    if ( app_settings.tool_icon_size > 0
                        && app_settings.tool_icon_size <= GTK_ICON_SIZE_DIALOG )
        gtk_toolbar_set_icon_size( GTK_TOOLBAR( file_browser->toolbar ), app_settings.tool_icon_size );

    // fill left toolbar
    xset_fill_toolbar( GTK_WIDGET( file_browser ), file_browser, file_browser->toolbar, xset_get_panel( p, "tool_l" ), show_tooltips );

    // add pathbar
    GtkWidget* hbox = gtk_hbox_new( FALSE, 0 );
    GtkToolItem* toolitem = gtk_tool_item_new();
    gtk_tool_item_set_expand ( toolitem, TRUE );
    gtk_toolbar_insert( GTK_TOOLBAR( file_browser->toolbar ), toolitem, -1 );
    gtk_container_add ( GTK_CONTAINER ( toolitem ), hbox );
    gtk_box_pack_start( GTK_BOX ( hbox ), GTK_WIDGET( file_browser->path_bar ), TRUE, TRUE, 5 );

    // fill right toolbar
    xset_fill_toolbar( GTK_WIDGET( file_browser ), file_browser, file_browser->toolbar, xset_get_panel( p, "tool_r" ), show_tooltips );

    // show
    if ( xset_get_b_panel_mode( p, "show_toolbox", mode ) )
        gtk_widget_show_all( file_browser->toolbox );
}


static void rebuild_side_toolbox( GtkWidget* widget, PtkFileBrowser* file_browser ) {
    FMMainWindow* main_window = (FMMainWindow*)file_browser->main_window;
    int p = file_browser->mypanel;
    char mode = main_window ? main_window->panel_context[p-1] : 0;

    gboolean show_tooltips = !xset_get_b_panel( 1, "tool_l" );

    // destroy
    if ( file_browser->side_toolbar )
        gtk_widget_destroy( file_browser->side_toolbar );

    // create side toolbar
    file_browser->side_toolbar = gtk_toolbar_new();

    gtk_box_pack_start( GTK_BOX( file_browser->side_toolbox ), file_browser->side_toolbar, TRUE, TRUE, 0 );
    gtk_toolbar_set_style( GTK_TOOLBAR( file_browser->side_toolbar ), GTK_TOOLBAR_ICONS );
    if ( app_settings.tool_icon_size > 0
                        && app_settings.tool_icon_size <= GTK_ICON_SIZE_DIALOG )
        gtk_toolbar_set_icon_size( GTK_TOOLBAR( file_browser->side_toolbar ), app_settings.tool_icon_size );
    // fill side toolbar
    xset_fill_toolbar( GTK_WIDGET( file_browser ), file_browser, file_browser->side_toolbar, xset_get_panel( p, "tool_s" ), show_tooltips );

    // show
    if ( xset_get_b_panel_mode( p, "show_sidebar", mode ) )
        gtk_widget_show_all( file_browser->side_toolbox );
}


void ptk_file_browser_rebuild_toolbars( PtkFileBrowser* file_browser ) {
    char* disp_path;
    int i;
    for ( i = 0; i < G_N_ELEMENTS( file_browser->toolbar_widgets ); i++ )
    {
        g_slist_free( file_browser->toolbar_widgets[i] );
        file_browser->toolbar_widgets[i] = NULL;
    }
    if ( file_browser->toolbar )
    {
        rebuild_toolbox( NULL, file_browser );
        disp_path = g_filename_display_name( ptk_file_browser_get_cwd( file_browser ) );
        gtk_entry_set_text( GTK_ENTRY( file_browser->path_bar ), disp_path );
        g_free( disp_path );
    }
    if ( file_browser->side_toolbar )
        rebuild_side_toolbox( NULL, file_browser );

    enable_toolbar( file_browser );
}


void ptk_file_browser_status_change( PtkFileBrowser* file_browser, gboolean panel_focus ) {
    char* scolor;
    GdkColor color;

    // image
    gtk_widget_set_sensitive( GTK_WIDGET( file_browser->status_image ), panel_focus );

    // text color
    if ( panel_focus )
    {
        scolor = xset_get_s( "status_text" );
        if ( scolor && gdk_color_parse( scolor, &color ) )
            gtk_widget_modify_fg( GTK_WIDGET( file_browser->status_label ), GTK_STATE_NORMAL, &color );
        else
            gtk_widget_modify_fg( GTK_WIDGET( file_browser->status_label ), GTK_STATE_NORMAL, NULL );
    }
    else
        gtk_widget_modify_fg( GTK_WIDGET( file_browser->status_label ), GTK_STATE_NORMAL, NULL );

    // frame border color
    if ( panel_focus )
    {
        scolor = xset_get_s( "status_border" );
        if ( scolor && gdk_color_parse( scolor, &color ) )
            gtk_widget_modify_bg( GTK_WIDGET( file_browser->status_frame ), GTK_STATE_NORMAL, &color );
        else
            gtk_widget_modify_bg( GTK_WIDGET( file_browser->status_frame ), GTK_STATE_NORMAL, NULL );
            // below caused visibility issues with some themes
            //gtk_widget_modify_bg( file_browser->status_frame, GTK_STATE_NORMAL,  &GTK_WIDGET( file_browser->status_frame )
            //                            ->style->fg[ GTK_STATE_SELECTED ] );
    }
    else
        gtk_widget_modify_bg( GTK_WIDGET( file_browser->status_frame ), GTK_STATE_NORMAL, NULL );
}


gboolean on_status_bar_button_press( GtkWidget *widget, GdkEventButton *event, PtkFileBrowser* file_browser ) {
    focus_folder_view( file_browser );
    if ( event->type == GDK_BUTTON_PRESS )
    {
        if ( ( evt_win_click->s || evt_win_click->ob2_data ) &&
                main_window_event( file_browser->main_window, evt_win_click, "evt_win_click", 0, 0, "statusbar", 0, event->button, event->state, TRUE ) )
            return TRUE;
        if ( event->button == 2 )
        {
            const char* setname[] =
            {
                "status_name",
                "status_path",
                "status_info",
                "status_hide"
            };
            int i;
            for ( i = 0; i < G_N_ELEMENTS( setname ); i++ )
            {
                if ( xset_get_b( setname[i] ) )
                {
                    if ( i < 2 )
                    {
                        GList* sel_files = ptk_file_browser_get_selected_files( file_browser );
                        if ( !sel_files )
                            return TRUE;
                        if ( i == 0 )
                            ptk_clipboard_copy_name( ptk_file_browser_get_cwd( file_browser ), sel_files );
                        else
                            ptk_clipboard_copy_as_text( ptk_file_browser_get_cwd( file_browser ), sel_files );
                        g_list_foreach( sel_files, ( GFunc ) vfs_file_info_unref, NULL );
                        g_list_free( sel_files );
                    }
                    else if ( i == 2 )
                        ptk_file_browser_file_properties( file_browser, 0 );
                    else if ( i == 3 )
                        focus_panel( NULL, file_browser->main_window, -3 );
                }
            }
            return TRUE;
        }
    }
    return FALSE;
}


void on_status_effect_change( GtkMenuItem* item, PtkFileBrowser* file_browser ) {
    main_update_fonts( NULL, file_browser );
    set_panel_focus( NULL, file_browser );
}


void on_status_middle_click_config( GtkMenuItem *menuitem, XSet* set ) {
    const char* setname[] =
    {
        "status_name",
        "status_path",
        "status_info",
        "status_hide"
    };
    int i;
    for ( i = 0; i < G_N_ELEMENTS( setname ); i++ )
    {
        if ( !strcmp( set->name, setname[i] ) )
            set->b = XSET_B_TRUE;
        else
            xset_set_b( setname[i], FALSE );
    }
}


void on_status_bar_popup( GtkWidget *widget, GtkWidget *menu, PtkFileBrowser* file_browser ) {
    XSet* set_radio;
    XSetContext* context = xset_context_new();
    main_context_fill( file_browser, context );
    GtkAccelGroup* accel_group = gtk_accel_group_new();
    char* desc = g_strdup_printf( "sep_bar1 status_border status_text panel%d_icon_status panel%d_font_status status_middle", file_browser->mypanel, file_browser->mypanel );

    xset_set_cb( "status_border", on_status_effect_change, file_browser );
    xset_set_cb( "status_text", on_status_effect_change, file_browser );
    xset_set_cb_panel( file_browser->mypanel, "icon_status", on_status_effect_change, file_browser );
    xset_set_cb_panel( file_browser->mypanel, "font_status", on_status_effect_change, file_browser );
    XSet* set = xset_get( "status_name" );
    xset_set_cb( "status_name", on_status_middle_click_config, set );
    xset_set_ob2( set, NULL, NULL );
    set_radio = set;
    set = xset_get( "status_path" );
    xset_set_cb( "status_path", on_status_middle_click_config, set );
    xset_set_ob2( set, NULL, set_radio );
    set = xset_get( "status_info" );
    xset_set_cb( "status_info", on_status_middle_click_config, set );
    xset_set_ob2( set, NULL, set_radio );
    set = xset_get( "status_hide" );
    xset_set_cb( "status_hide", on_status_middle_click_config, set );
    xset_set_ob2( set, NULL, set_radio );

    xset_add_menu( NULL, file_browser, menu, accel_group, desc );
    g_free( desc );
    gtk_widget_show_all( menu );
    g_signal_connect( menu, "key-press-event", G_CALLBACK( xset_menu_keypress ), NULL );
}


/*
static gboolean on_status_bar_key_press( GtkWidget* widget, GdkEventKey* event, gpointer user_data) {
    printf( "on_status_bar_key_press\n");
    return FALSE;
}
*/

/*
void on_side_vbox_allocate( GtkWidget* widget, GdkRectangle* allocation, PtkFileBrowser* file_browser ) {
    //printf("side_vbox: %d, %d\n", allocation->width, allocation->height );

}

void on_paned_allocate( GtkWidget* widget, GdkRectangle* allocation, PtkFileBrowser* file_browser ) {
    int pos;
    FMMainWindow* main_window = (FMMainWindow*)file_browser->main_window;

    if ( widget == file_browser->side_vpane_top )
        pos = main_window->panel_slide_y[file_browser->mypanel-1];
    else if ( widget == file_browser->side_vpane_bottom )
        pos = main_window->panel_slide_s[file_browser->mypanel-1];
    else
        return;

    printf("paned: %d, %d -- %d\n", allocation->width, allocation->height, pos );

    //gtk_paned_set_position( GTK_PANED(widget), pos );
}

gboolean on_slider_change( GtkWidget *widget, GdkEvent  *event, // correct?
                                                PtkFileBrowser* file_browser ) {
    if ( !file_browser )
        return;
    FMMainWindow* main_window = (FMMainWindow*)file_browser->main_window;

    printf("slider_change\n");
    main_window->panel_slide_y[file_browser->mypanel-1] = gtk_paned_get_position( GTK_PANED( file_browser->side_vpane_top ) );
    main_window->panel_slide_s[file_browser->mypanel-1] = gtk_paned_get_position( GTK_PANED( file_browser->side_vpane_bottom ) );
    return FALSE;
}
*/


void ptk_file_browser_init( PtkFileBrowser* file_browser ) {
    file_browser->mypanel = 0;  // don't load font yet in ptk_path_entry_new
    file_browser->path_bar = ptk_path_entry_new( file_browser );
    g_signal_connect( file_browser->path_bar, "activate", G_CALLBACK(on_address_bar_activate), file_browser );
    g_signal_connect( file_browser->path_bar, "focus-in-event", G_CALLBACK(on_address_bar_focus_in), file_browser );

    // toolbox
    file_browser->toolbar = NULL;
    file_browser->toolbox = gtk_hbox_new( FALSE, 0 );
    gtk_box_pack_start( GTK_BOX ( file_browser ), file_browser->toolbox, FALSE, FALSE, 0 );

    // lists area
    file_browser->hpane = gtk_hpaned_new();
    file_browser->side_vbox = gtk_vbox_new( FALSE, 0 );
    gtk_widget_set_size_request( file_browser->side_vbox, 140, -1 );
    file_browser->folder_view_scroll = gtk_scrolled_window_new( NULL, NULL );
    gtk_paned_pack1 ( GTK_PANED( file_browser->hpane ), file_browser->side_vbox, FALSE, FALSE );
    gtk_paned_pack2 ( GTK_PANED( file_browser->hpane ), file_browser->folder_view_scroll, TRUE, TRUE );

    // fill side
    file_browser->side_toolbox = gtk_hbox_new( FALSE, 0 );
    file_browser->side_toolbar = NULL;
    file_browser->side_vpane_top = gtk_vpaned_new();
    file_browser->side_vpane_bottom = gtk_vpaned_new();
    file_browser->side_dir_scroll = gtk_scrolled_window_new( NULL, NULL );
    file_browser->side_book_scroll = gtk_scrolled_window_new( NULL, NULL );
    file_browser->side_dev_scroll = gtk_scrolled_window_new( NULL, NULL );
    gtk_box_pack_start ( GTK_BOX( file_browser->side_vbox ), file_browser->side_toolbox, FALSE, FALSE, 0 );
    gtk_box_pack_start ( GTK_BOX( file_browser->side_vbox ), file_browser->side_vpane_top, TRUE, TRUE, 0 );
#if GTK_CHECK_VERSION (3, 0, 0)
    // see     /issues/21
    gtk_paned_pack1 ( GTK_PANED( file_browser->side_vpane_top ), file_browser->side_dev_scroll, FALSE, FALSE );
    gtk_paned_pack2 ( GTK_PANED( file_browser->side_vpane_top ), file_browser->side_vpane_bottom, TRUE, FALSE );
    gtk_paned_pack1 ( GTK_PANED( file_browser->side_vpane_bottom ), file_browser->side_book_scroll, FALSE, FALSE );
    gtk_paned_pack2 ( GTK_PANED( file_browser->side_vpane_bottom ), file_browser->side_dir_scroll, TRUE, FALSE );
#else
    gtk_paned_pack1 ( GTK_PANED( file_browser->side_vpane_top ), file_browser->side_dev_scroll, FALSE, TRUE );
    gtk_paned_pack2 ( GTK_PANED( file_browser->side_vpane_top ), file_browser->side_vpane_bottom, TRUE, TRUE );
    gtk_paned_pack1 ( GTK_PANED( file_browser->side_vpane_bottom ), file_browser->side_book_scroll, FALSE, TRUE );
    gtk_paned_pack2 ( GTK_PANED( file_browser->side_vpane_bottom ), file_browser->side_dir_scroll, TRUE, TRUE );
#endif

    // status bar                    //howdy
    file_browser->status_bar = gtk_statusbar_new();

    GList* children = gtk_container_get_children( GTK_CONTAINER( file_browser->status_bar ) );
    file_browser->status_frame = GTK_FRAME( children->data );
    g_list_free( children );
    children = gtk_container_get_children( GTK_CONTAINER( gtk_statusbar_get_message_area( GTK_STATUSBAR( file_browser->status_bar ) ) ) );
    file_browser->status_label = GTK_LABEL( children->data );
    g_list_free( children );
     //don't yet know which panel here
    file_browser->status_image = xset_get_image( "gtk-yes", GTK_ICON_SIZE_MENU );
    gtk_box_pack_start ( GTK_BOX( file_browser->status_bar ), file_browser->status_image, FALSE, FALSE, 0 );
    // required for button event
    gtk_label_set_selectable( file_browser->status_label, TRUE );
    gtk_widget_set_can_focus( GTK_WIDGET( file_browser->status_label ), FALSE );
#if GTK_CHECK_VERSION (3, 0, 0)
    gtk_widget_set_hexpand( GTK_WIDGET( file_browser->status_label ), TRUE );
    gtk_widget_set_halign( GTK_WIDGET( file_browser->status_label ), GTK_ALIGN_FILL );
    gtk_misc_set_alignment( GTK_MISC( file_browser->status_label ), 0, 0.5 );
#endif
    g_signal_connect( G_OBJECT( file_browser->status_label ), "button-press-event", G_CALLBACK( on_status_bar_button_press ), file_browser );
    g_signal_connect( G_OBJECT( file_browser->status_label ), "populate-popup", G_CALLBACK( on_status_bar_popup ), file_browser );
    //g_signal_connect( G_OBJECT( file_browser->status_label ), "key-press-event", G_CALLBACK( on_status_bar_key_press ), file_browser );
    if ( xset_get_s_panel( file_browser->mypanel, "font_status" ) )
    {
        PangoFontDescription* font_desc = pango_font_description_from_string( xset_get_s_panel( file_browser->mypanel, "font_status" ) );
        gtk_widget_modify_font( GTK_WIDGET( file_browser->status_label ), font_desc );
        pango_font_description_free( font_desc );
    }

    // pack fb vbox
    gtk_box_pack_start( GTK_BOX ( file_browser ), file_browser->hpane, TRUE, TRUE, 0 );
    // TODO pack task frames
    gtk_box_pack_start( GTK_BOX ( file_browser ), file_browser->status_bar, FALSE, FALSE, 0 );

    gtk_scrolled_window_set_policy ( GTK_SCROLLED_WINDOW ( file_browser->folder_view_scroll ), GTK_POLICY_AUTOMATIC, GTK_POLICY_ALWAYS );
    gtk_scrolled_window_set_policy( GTK_SCROLLED_WINDOW( file_browser->side_dir_scroll ), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC );
    gtk_scrolled_window_set_policy( GTK_SCROLLED_WINDOW( file_browser->side_book_scroll ), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC );
    gtk_scrolled_window_set_policy( GTK_SCROLLED_WINDOW( file_browser->side_dev_scroll ), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC );

    g_signal_connect( file_browser->hpane, "button-release-event", G_CALLBACK( ptk_file_browser_slider_release ), file_browser );
    g_signal_connect( file_browser->side_vpane_top, "button-release-event", G_CALLBACK( ptk_file_browser_slider_release ), file_browser );
    g_signal_connect( file_browser->side_vpane_bottom, "button-release-event", G_CALLBACK( ptk_file_browser_slider_release ), file_browser );
/*
    g_signal_connect( file_browser->side_vbox, "size-allocate", G_CALLBACK( on_side_vbox_allocate ), file_browser );
    g_signal_connect( file_browser->side_vpane_top, "size-allocate", G_CALLBACK( on_paned_allocate ), file_browser );
    g_signal_connect( file_browser->side_vpane_bottom, "size-allocate", G_CALLBACK( on_paned_allocate ), file_browser );
*/

/*
    // these work but fire too often
    g_signal_connect( file_browser->hpane, "notify::position", G_CALLBACK( on_slider_change ), file_browser );
    g_signal_connect( file_browser->side_vpane_top, "notify::position", G_CALLBACK( on_slider_change ), file_browser );
    g_signal_connect( file_browser->side_vpane_bottom, "notify::position", G_CALLBACK( on_slider_change ), file_browser );
*/
}


void ptk_file_browser_finalize( GObject *obj ) {
    int i;
    PtkFileBrowser * file_browser = PTK_FILE_BROWSER( obj );
//printf("ptk_file_browser_finalize\n");
    if ( file_browser->dir )
    {
        g_signal_handlers_disconnect_matched( file_browser->dir, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, file_browser );
        g_object_unref( file_browser->dir );
    }

    /* Remove all idle handlers which are not called yet. */
    do
    {}
    while ( g_source_remove_by_user_data( file_browser ) );

    if ( file_browser->file_list )
    {
        g_signal_handlers_disconnect_matched( file_browser->file_list, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, file_browser );
        g_object_unref( G_OBJECT( file_browser->file_list ) );
    }

    g_free( file_browser->status_bar_custom );
    g_free( file_browser->seek_name );
    file_browser->seek_name = NULL;
    g_free( file_browser->book_set_name );
    file_browser->book_set_name = NULL;
    g_free( file_browser->select_path );
    file_browser->select_path = NULL;
    for ( i = 0; i < G_N_ELEMENTS( file_browser->toolbar_widgets ); i++ )
    {
        g_slist_free( file_browser->toolbar_widgets[i] );
        file_browser->toolbar_widgets[i] = NULL;
    }

    G_OBJECT_CLASS( parent_class ) ->finalize( obj );

    /* Ensuring free space at the end of the heap is freed to the OS,
     * mainly to deal with the possibility that killing the browser results in
     * thousands of large thumbnails being freed, but the memory not actually
     * released by zzzFM */
#if defined (__GLIBC__)
    malloc_trim(0);
#endif
}


void ptk_file_browser_get_property ( GObject *obj, guint prop_id, GValue *value, GParamSpec *pspec ) {}

void ptk_file_browser_set_property ( GObject *obj, guint prop_id, const GValue *value, GParamSpec *pspec ) {}


void ptk_file_browser_update_views( GtkWidget* item, PtkFileBrowser* file_browser ) {
    int i;
//printf("ptk_file_browser_update_views fb=%p  (panel %d)\n", file_browser, file_browser->mypanel );

    FMMainWindow* main_window = (FMMainWindow*)file_browser->main_window;
    // hide/show browser widgets based on user settings
    int p = file_browser->mypanel;
    char mode = main_window->panel_context[p-1];
    gboolean need_enable_toolbar = FALSE;

    if ( xset_get_b_panel_mode( p, "show_toolbox", mode ) )
    {
        if ( ( evt_pnl_show->s || evt_pnl_show->ob2_data ) &&
                            ( !file_browser->toolbar ||
                            !gtk_widget_get_visible( file_browser->toolbox ) ) )
            main_window_event( main_window, evt_pnl_show, "evt_pnl_show", 0, 0, "toolbar", 0, 0, 0, TRUE );
        if ( !file_browser->toolbar )
        {
            rebuild_toolbox( NULL, file_browser );
            need_enable_toolbar = TRUE;
        }
        gtk_widget_show_all( file_browser->toolbox );
    } else {
        if ( ( evt_pnl_show->s || evt_pnl_show->ob2_data ) &&
                            file_browser->toolbox &&
                            gtk_widget_get_visible( file_browser->toolbox ) )
            main_window_event( main_window, evt_pnl_show, "evt_pnl_show", 0, 0, "toolbar", 0, 0, 0, FALSE );
        gtk_widget_hide( file_browser->toolbox );
    }

    if ( xset_get_b_panel_mode( p, "show_sidebar", mode ) )
    {
        if ( ( evt_pnl_show->s || evt_pnl_show->ob2_data ) &&
                            ( !file_browser->side_toolbox ||
                            !gtk_widget_get_visible( file_browser->side_toolbox ) ) )
            main_window_event( main_window, evt_pnl_show, "evt_pnl_show", 0, 0, "sidetoolbar", 0, 0, 0, TRUE );
        if ( !file_browser->side_toolbar )
        {
            rebuild_side_toolbox( NULL, file_browser );
            need_enable_toolbar = TRUE;
        }
        gtk_widget_show_all( file_browser->side_toolbox );
    } else {
        if ( ( evt_pnl_show->s || evt_pnl_show->ob2_data ) &&
                            file_browser->side_toolbar &&
                            file_browser->side_toolbox &&
                            gtk_widget_get_visible( file_browser->side_toolbox ) )
            main_window_event( main_window, evt_pnl_show, "evt_pnl_show", 0, 0, "sidetoolbar", 0, 0, 0, FALSE );
        /*  toolboxes must be destroyed together for toolbar_widgets[]
        if ( file_browser->side_toolbar )
        {
            gtk_widget_destroy( file_browser->side_toolbar );
            file_browser->side_toolbar = NULL;
        }
        */
        gtk_widget_hide( file_browser->side_toolbox );
    }

    if ( xset_get_b_panel_mode( p, "show_dirtree", mode ) )
    {
        if ( ( evt_pnl_show->s || evt_pnl_show->ob2_data ) &&
                        ( !file_browser->side_dir_scroll ||
                        !gtk_widget_get_visible( file_browser->side_dir_scroll ) ) )
            main_window_event( main_window, evt_pnl_show, "evt_pnl_show", 0, 0, "dirtree", 0, 0, 0, TRUE );
        if ( !file_browser->side_dir )
        {
            file_browser->side_dir = ptk_file_browser_create_dir_tree( file_browser );
            gtk_container_add( GTK_CONTAINER( file_browser->side_dir_scroll ), file_browser->side_dir );
        }
        gtk_widget_show_all( file_browser->side_dir_scroll );
        if ( file_browser->side_dir && file_browser->file_list )
            ptk_dir_tree_view_chdir( GTK_TREE_VIEW( file_browser->side_dir ), ptk_file_browser_get_cwd( file_browser ) );
    } else {
        if ( ( evt_pnl_show->s || evt_pnl_show->ob2_data ) &&
                            file_browser->side_dir_scroll &&
                            gtk_widget_get_visible( file_browser->side_dir_scroll ) )
            main_window_event( main_window, evt_pnl_show, "evt_pnl_show", 0, 0, "dirtree", 0, 0, 0, FALSE );
        gtk_widget_hide( file_browser->side_dir_scroll );
        if ( file_browser->side_dir )
            gtk_widget_destroy( file_browser->side_dir );
        file_browser->side_dir = NULL;
    }

    if ( xset_get_b_panel_mode( p, "show_book", mode ) )
    {
        if ( ( evt_pnl_show->s || evt_pnl_show->ob2_data ) &&
                        ( !file_browser->side_book_scroll ||
                        !gtk_widget_get_visible( file_browser->side_book_scroll ) ) )
            main_window_event( main_window, evt_pnl_show, "evt_pnl_show", 0, 0, "bookmarks", 0, 0, 0, TRUE );
        if ( !file_browser->side_book )
        {
            file_browser->side_book = ptk_bookmark_view_new( file_browser );
            gtk_container_add( GTK_CONTAINER( file_browser->side_book_scroll ), file_browser->side_book );
        }
        gtk_widget_show_all( file_browser->side_book_scroll );
    } else {
        if ( ( evt_pnl_show->s || evt_pnl_show->ob2_data ) &&
                            file_browser->side_book_scroll &&
                            gtk_widget_get_visible( file_browser->side_book_scroll ) )
            main_window_event( main_window, evt_pnl_show, "evt_pnl_show", 0, 0, "bookmarks", 0, 0, 0, FALSE );
        gtk_widget_hide( file_browser->side_book_scroll );
        if ( file_browser->side_book )
            gtk_widget_destroy( file_browser->side_book );
        file_browser->side_book = NULL;
    }

    if ( xset_get_b_panel_mode( p, "show_devmon", mode ) )
    {
        if ( ( evt_pnl_show->s || evt_pnl_show->ob2_data ) &&
                        ( !file_browser->side_dev_scroll ||
                        !gtk_widget_get_visible( file_browser->side_dev_scroll ) ) )
            main_window_event( main_window, evt_pnl_show, "evt_pnl_show", 0, 0, "devices", 0, 0, 0, TRUE );
        if ( !file_browser->side_dev )
        {
            file_browser->side_dev = ptk_location_view_new( file_browser );
            gtk_container_add( GTK_CONTAINER( file_browser->side_dev_scroll ), file_browser->side_dev );
        }
        gtk_widget_show_all( file_browser->side_dev_scroll );
    } else {
        if ( ( evt_pnl_show->s || evt_pnl_show->ob2_data ) &&
                            file_browser->side_dev_scroll &&
                            gtk_widget_get_visible( file_browser->side_dev_scroll ) )
            main_window_event( main_window, evt_pnl_show, "evt_pnl_show", 0, 0, "devices", 0, 0, 0, FALSE );
        gtk_widget_hide( file_browser->side_dev_scroll );
        if ( file_browser->side_dev )
            gtk_widget_destroy( file_browser->side_dev );
        file_browser->side_dev = NULL;
    }

    if ( xset_get_b_panel_mode( p, "show_book", mode ) ||
                            xset_get_b_panel_mode( p, "show_dirtree", mode ) )
        gtk_widget_show( file_browser->side_vpane_bottom );
    else
        gtk_widget_hide( file_browser->side_vpane_bottom );

    if ( xset_get_b_panel_mode( p, "show_devmon", mode ) ||
                            xset_get_b_panel_mode( p, "show_dirtree", mode ) ||
                            xset_get_b_panel_mode( p, "show_book", mode ) )
        gtk_widget_show( file_browser->side_vbox );
    else
        gtk_widget_hide( file_browser->side_vbox );

    if ( need_enable_toolbar )
        enable_toolbar( file_browser );
    else
    {
        // toggle sidepane toolbar buttons
        ptk_file_browser_update_toolbar_widgets( file_browser, NULL, XSET_TOOL_DEVICES );
        ptk_file_browser_update_toolbar_widgets( file_browser, NULL, XSET_TOOL_BOOKMARKS );
        ptk_file_browser_update_toolbar_widgets( file_browser, NULL, XSET_TOOL_TREE );
    }

    // set slider positions
    int pos;
    // hpane
    pos = main_window->panel_slide_x[p-1];
    if ( pos < 100 ) pos = -1;
//printf( "    set slide_x = %d  \n", pos );
    if ( pos > 0 )
        gtk_paned_set_position( GTK_PANED( file_browser->hpane ), pos );

    // side_vpane_top
    pos = main_window->panel_slide_y[p-1];
    if ( pos < 20 ) pos = -1;
//printf( "    slide_y = %d  ", pos );
    gtk_paned_set_position( GTK_PANED( file_browser->side_vpane_top ), pos );

    // side_vpane_bottom
    pos = main_window->panel_slide_s[p-1];
    if ( pos < 20 ) pos = -1;
//printf( "slide_s = %d\n", pos );
    gtk_paned_set_position( GTK_PANED( file_browser->side_vpane_bottom ), pos );

    // Large Icons - option for Detailed and Compact list views
    gboolean large_icons = xset_get_b_panel_mode( p, "list_large", mode );
    if ( large_icons != !!file_browser->large_icons )
    {
        if ( file_browser->folder_view )
        {
            // force rebuild of folder_view for icon size change
            gtk_widget_destroy( file_browser->folder_view );
            file_browser->folder_view = NULL;
        }
        file_browser->large_icons = large_icons;
        ptk_file_browser_update_toolbar_widgets( file_browser, NULL, XSET_TOOL_LARGE_ICONS );
    }

    // List Styles
    if ( xset_get_b_panel( p, "list_detailed" ) )
    {
        ptk_file_browser_view_as_list( file_browser );

        // Set column widths for this panel context
        GtkTreeViewColumn* col;
        //GtkTreeViewColumn* name_col = NULL;
        int j, width;
        //int total_width = 0;
        //int minor_width = 0;
        const char* title;
        XSet* set;
        //GtkAllocation allocation;

        if ( GTK_IS_TREE_VIEW( file_browser->folder_view ) )
        {
//printf("    set widths   mode = %d\n", mode);
            for ( i = 0; i < 6; i++ )
            {
                col = gtk_tree_view_get_column( GTK_TREE_VIEW( file_browser->folder_view ), i );
                if ( !col )
                    break;
                title = gtk_tree_view_column_get_title( col );
                for ( j = 0; j < 6; j++ )
                {
                    if ( !strcmp( title, _(column_titles[j]) ) )
                        break;
                }
                if ( j != 6 )
                {
                    // get column width for this panel context
                    set = xset_get_panel_mode( p, column_names[j], mode );
                    width = set->y ? atoi( set->y ) : 100;
//printf("        %d\t%s\n", width, title );
                    if ( width )
                    {
                        gtk_tree_view_column_set_fixed_width( col, width );
                        //printf("upd set_width %s %d\n", column_names[j], width );
                        /*
                        if ( set->b == XSET_B_TRUE )
                        {
                            total_width += width;
                            if ( j != 0 )
                                minor_width += width;
                            else
                                name_col = col;
                        }
                        */
                    }
                    // set column visibility
                    gtk_tree_view_column_set_visible( col, set->b == XSET_B_TRUE || j == 0 );
                }
            }
            /* This breaks panel memory, eg:
             * turn on panel 3, Name+Size cols, turn off 3, turn on 3, Size column goes to min
             * panels 1+2 on, turn off 1, turn off 2, turn on 2, column widths in panel2 go to minimums
             * Name column is expanding?         //   howdy   known issue

            gtk_widget_get_allocation( file_browser->folder_view, &allocation );
            //printf("list_view width %d\n", allocation.width );
            if ( total_width < allocation.width && name_col )
            {
                // prevent Name column from auto-expanding
                // If total column widths are less than treeview allocation width
                // the Name column expands and won't allow user to downsize
                gtk_tree_view_column_set_fixed_width( name_col, allocation.width - minor_width );
                set = xset_get_panel_mode( p, column_names[0], mode );
                g_free( set->y );
                set->y = g_strdup_printf( "%d", allocation.width - minor_width );
                //printf("name col width reset %d\n", allocation.width - minor_width );
            }
            */
        }
    }
    else if ( xset_get_b_panel( p, "list_compact" ) )
        ptk_file_browser_view_as_compact_list( file_browser );
    else
    {
        xset_set_panel( p, "list_detailed", "b", "1" );
        ptk_file_browser_view_as_list( file_browser );
    }

    // Show Hidden
    ptk_file_browser_show_hidden_files( file_browser, xset_get_b_panel( p, "show_hidden" ) );

//printf("ptk_file_browser_update_views fb=%p DONE\n", file_browser);
}


GtkWidget* ptk_file_browser_new( int curpanel, GtkWidget* notebook, GtkWidget* task_view, gpointer main_window ) {
    int i;
    PtkFileBrowser * file_browser;
    PtkFBViewMode view_mode;
    PangoFontDescription* font_desc;
    file_browser = ( PtkFileBrowser* ) g_object_new( PTK_TYPE_FILE_BROWSER, NULL );

    file_browser->mypanel = curpanel;
    file_browser->mynotebook = notebook;
    file_browser->main_window = main_window;
    file_browser->task_view = task_view;
    file_browser->sel_change_idle = 0;
    file_browser->inhibit_focus = file_browser->busy = FALSE;
    file_browser->seek_name = NULL;
    file_browser->book_set_name = NULL;
    for ( i = 0; i < G_N_ELEMENTS( file_browser->toolbar_widgets ); i++ )
        file_browser->toolbar_widgets[i] = NULL;

    if ( xset_get_b_panel( curpanel, "list_detailed" ) )
        view_mode = PTK_FB_LIST_VIEW;
    else if ( xset_get_b_panel( curpanel, "list_compact" ) )
    {
        view_mode = PTK_FB_COMPACT_VIEW;
        gtk_scrolled_window_set_policy( GTK_SCROLLED_WINDOW( file_browser->folder_view_scroll ), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC );
    } else {
        xset_set_panel( curpanel, "list_detailed", "b", "1" );
        view_mode = PTK_FB_LIST_VIEW;
    }

    file_browser->view_mode = view_mode;  //sfm was after next line
    // Large Icons - option for Detailed and Compact list views
    file_browser->large_icons = view_mode ==  xset_get_b_panel_mode( file_browser->mypanel, "list_large",
        ((FMMainWindow*)main_window)->panel_context[file_browser->mypanel-1] );
    file_browser->folder_view = create_folder_view( file_browser, view_mode );

    gtk_container_add ( GTK_CONTAINER ( file_browser->folder_view_scroll ), file_browser->folder_view );

    file_browser->side_dir = NULL;
    file_browser->side_book = NULL;
    file_browser->side_dev = NULL;

    file_browser->select_path = NULL;
    file_browser->status_bar_custom = NULL;

    //gtk_widget_show_all( file_browser->folder_view_scroll );

    // set status bar icon
    char* icon_name;
    XSet* set = xset_get_panel( curpanel, "icon_status" );
    if ( set->icon && set->icon[0] != '\0' )
        icon_name = set->icon;
    else
        icon_name = "gtk-yes";
    gtk_image_set_from_icon_name( GTK_IMAGE( file_browser->status_image ), icon_name, GTK_ICON_SIZE_MENU );
    // set status bar font
    char* fontname = xset_get_s_panel( curpanel, "font_status" );
    if ( fontname )
    {
        font_desc = pango_font_description_from_string( fontname );
        gtk_widget_modify_font( GTK_WIDGET( file_browser->status_label ), font_desc );
        pango_font_description_free( font_desc );
    }

    // set path bar font (is created before mypanel is set)
    if ( file_browser->path_bar &&
                        ( fontname = xset_get_s_panel( curpanel, "font_path" ) ) )
    {
        font_desc = pango_font_description_from_string( fontname );
        gtk_widget_modify_font( GTK_WIDGET( file_browser->path_bar ), font_desc );
        pango_font_description_free( font_desc );
    }

    gtk_widget_show_all( GTK_WIDGET( file_browser ) );

    //ptk_file_browser_update_views( NULL, file_browser );

    return GTK_IS_WIDGET( file_browser ) ? ( GtkWidget* ) file_browser : NULL;
}


gboolean ptk_file_restrict_homedir( const char* folder_path ) {
    const char *homedir = NULL;
    int ret=(1==0);

    homedir = g_getenv("HOME");
    if (!homedir) {
      homedir = g_get_home_dir();
    }
    if (g_str_has_prefix(folder_path,homedir)) {
      ret=(1==1);
    }
    if (g_str_has_prefix(folder_path,"/media")) {
      ret=(1==1);
    }
    return ret;
}


void ptk_file_browser_update_tab_label( PtkFileBrowser* file_browser ) {
    GtkWidget * label;
    GtkContainer* hbox;
    GtkImage* icon;
    GtkLabel* text;
    GList* children;
    gchar* name;

    label = gtk_notebook_get_tab_label ( GTK_NOTEBOOK( file_browser->mynotebook ), GTK_WIDGET( file_browser ) );
    hbox = GTK_CONTAINER( gtk_bin_get_child ( GTK_BIN( label ) ) );
    children = gtk_container_get_children( hbox );
    icon = GTK_IMAGE( children->data );
    text = GTK_LABEL( children->next->data );
    g_list_free( children );

    /* TODO: Change the icon */

    name = g_path_get_basename( ptk_file_browser_get_cwd( file_browser ) );
    gtk_label_set_text( text, name );
#if GTK_CHECK_VERSION (3, 0, 0)
    gtk_label_set_ellipsize( text, PANGO_ELLIPSIZE_MIDDLE );
    if (strlen( name ) < 30)
    {
        gtk_label_set_ellipsize( text, PANGO_ELLIPSIZE_NONE );
        gtk_label_set_width_chars( text, -1 );
    }
    else
        gtk_label_set_width_chars( text, 30 );
#endif
    g_free( name );
}


void ptk_file_browser_select_last( PtkFileBrowser* file_browser )
{
//printf("ptk_file_browser_select_last\n");
    // select one file?
    if ( file_browser->select_path )
    {
        ptk_file_browser_select_file( file_browser, file_browser->select_path );
        g_free( file_browser->select_path );
        file_browser->select_path = NULL;
        return;
    }

    // select previously selected files
    gint elementn = -1;
    GList* l;
    GList* element = NULL;
    //printf("    search for %s\n", (char*)file_browser->curHistory->data );

    if ( file_browser->history && file_browser->histsel &&  file_browser->curHistory && ( l = g_list_last( file_browser->history ) ) )
    {
        if ( l->data && !strcmp( (char*)l->data, (char*)file_browser->curHistory->data ) )
        {
            elementn = g_list_position( file_browser->history, l );
            if ( elementn != -1 )
            {
                element = g_list_nth( file_browser->histsel, elementn );
                // skip the current history item if sellist empty since it was just created
                if ( !element->data )
                {
                    //printf( "        found current empty\n");
                    element = NULL;
                }
                //else printf( "        found current NON-empty\n");
            }
        }
        if ( !element )
        {
            while ( l = l->prev )
            {
                if ( l->data && !strcmp( (char*)l->data, (char*)file_browser->curHistory->data ) )
                {
                    elementn = g_list_position( file_browser->history, l );
                    //printf ("        found elementn=%d\n", elementn );
                    if ( elementn != -1 )
                        element = g_list_nth( file_browser->histsel, elementn );
                    break;
                }
            }
        }
    }

/*
    if ( element )
    {
        g_debug ("element OK" );
        if ( element->data )
            g_debug ("element->data OK" );
        else
            g_debug ("element->data NULL" );
    }
    else
        g_debug ("element NULL" );
    g_debug ("histsellen=%d", g_list_length( file_browser->histsel ) );
*/
    if ( element && element->data )
    {
        //printf("    select files\n");
        PtkFileList* list = PTK_FILE_LIST( file_browser->file_list );
        GtkTreeIter it;
        GtkTreePath* tp;
        GtkTreeSelection* tree_sel;
        gboolean firstsel = TRUE;
        if ( file_browser->view_mode == PTK_FB_LIST_VIEW )
            tree_sel = gtk_tree_view_get_selection( GTK_TREE_VIEW( file_browser->folder_view ) );
        for ( l = element->data; l; l = l->next )
        {
            if ( l->data )
            {
                //g_debug ("find a file");
                VFSFileInfo* file = l->data;
                if( ptk_file_list_find_iter( list, &it, file ) )
                {
                    //g_debug ("found file");
                    tp = gtk_tree_model_get_path( GTK_TREE_MODEL(list), &it );
                    if ( file_browser->view_mode == PTK_FB_COMPACT_VIEW )
                    {
                        exo_icon_view_select_path( EXO_ICON_VIEW( file_browser->folder_view ), tp );
                        if ( firstsel )
                        {
                            exo_icon_view_set_cursor( EXO_ICON_VIEW( file_browser->folder_view ), tp, NULL, FALSE );
                            exo_icon_view_scroll_to_path( EXO_ICON_VIEW( file_browser->folder_view ), tp, TRUE, .25, 0 );
                            firstsel = FALSE;
                        }
                    }
                    else if ( file_browser->view_mode == PTK_FB_LIST_VIEW )
                    {
                        gtk_tree_selection_select_path( tree_sel, tp );
                        if ( firstsel )
                        {
                            gtk_tree_view_set_cursor( GTK_TREE_VIEW( file_browser->folder_view ), tp, NULL, FALSE);
                            gtk_tree_view_scroll_to_cell( GTK_TREE_VIEW( file_browser->folder_view ), tp, NULL, TRUE, .25, 0 );
                            firstsel = FALSE;
                        }
                    }
                    gtk_tree_path_free( tp );
                }
            }
        }
    }
}

gboolean ptk_file_browser_chdir( PtkFileBrowser* file_browser, const char* folder_path, PtkFBChdirMode mode ) {
    gboolean cancel = FALSE;
    GtkWidget* folder_view = file_browser->folder_view;
//printf("ptk_file_browser_chdir\n");
    char* path_end;
    char* path;
    char* msg;

    gboolean inhibit_focus = file_browser->inhibit_focus;
    //file_browser->button_press = FALSE;
    file_browser->is_drag = FALSE;
    file_browser->menu_shown = FALSE;
    if ( file_browser->view_mode == PTK_FB_LIST_VIEW ||
                                                app_settings.single_click )
        /* sfm 1.0.6 don't reset skip_release for Icon/Compact to prevent file
           under cursor being selected when entering dir with double-click.
           Reset is conditional here to avoid possible but unlikely unintended breakage elsewhere. */
        file_browser->skip_release = FALSE;

    if ( ! folder_path )
        return FALSE;

    if ( folder_path )
    {
        path = strdup( folder_path );
        /* remove redundent '/' */
        if ( strcmp( path, "/" ) )
        {
            path_end = path + strlen( path ) - 1;
            for ( ; path_end > path; --path_end )
            {
                if ( *path_end != '/' )
                    break;
                else
                    *path_end = '\0';
            }
        }

        // convert ~ to /home/user for smarter bookmarks
        if ( g_str_has_prefix( path, "~/" ) || !g_strcmp0( path, "~" ) )
        {
            msg = g_strdup_printf( "%s%s", g_get_home_dir(), path + 1 );
            g_free( path );
            path = msg;
        }
    }
    else
        path = NULL;

    if ( ! path || ! g_file_test( path, ( G_FILE_TEST_IS_DIR ) ) )
    {
        if ( !inhibit_focus )
        {
            msg = g_strdup_printf( _("Directory doesn't exist\n\n%s"), path );
            ptk_show_error( GTK_WINDOW( gtk_widget_get_toplevel( GTK_WIDGET( file_browser ) ) ), _("Error"), msg );
            if ( path )
                g_free( path );
            g_free( msg );
        }
        return FALSE;
    }

    if ( !have_x_access( path ) )
    {
        if ( !inhibit_focus )
        {
            msg = g_strdup_printf( _("Unable to access %s\n\n%s"), path, g_markup_escape_text(g_strerror( errno ), -1) );
            ptk_show_error( GTK_WINDOW( gtk_widget_get_toplevel( GTK_WIDGET( file_browser ) ) ), _("Error"), msg );
            g_free(msg);
        }
        return FALSE;
    }

    g_signal_emit( file_browser, signals[ BEFORE_CHDIR_SIGNAL ], 0, path, &cancel );

    if ( cancel )
        return FALSE;

    //MOD remember selected files
    //g_debug ("@@@@@@@@@@@ remember: %s", ptk_file_browser_get_cwd( file_browser ) );
    if ( file_browser->curhistsel && file_browser->curhistsel->data )
    {
        //g_debug ("free curhistsel");
        g_list_foreach ( file_browser->curhistsel->data, ( GFunc ) vfs_file_info_unref, NULL );
        g_list_free( file_browser->curhistsel->data );
    }
    if ( file_browser->curhistsel )
    {
        file_browser->curhistsel->data =
                        ptk_file_browser_get_selected_files( file_browser );

        //g_debug("set curhistsel %d", g_list_position( file_browser->histsel,
        //                                    file_browser->curhistsel ) );
        //if ( file_browser->curhistsel->data )
        //    g_debug ("curhistsel->data OK" );
        //else
        //    g_debug ("curhistsel->data NULL" );

    }

    if ( mode == PTK_FB_CHDIR_ADD_HISTORY )
    {
        if ( ! file_browser->curHistory ||
                        strcmp( (char*)file_browser->curHistory->data, path ) )
        {
            /* Has forward history */
            if ( file_browser->curHistory && file_browser->curHistory->next )
            {
                /* clear old forward history */
                g_list_foreach ( file_browser->curHistory->next, ( GFunc ) g_free, NULL );
                g_list_free( file_browser->curHistory->next );
                file_browser->curHistory->next = NULL;
            }
            //MOD added - make histsel shadow file_browser->history
            if ( file_browser->curhistsel && file_browser->curhistsel->next )
            {
                //g_debug("@@@@@@@@@@@ free forward");
                GList* l;
                for ( l = file_browser->curhistsel->next; l; l = l->next )
                {
                    if ( l->data )
                    {
                        //g_debug("free forward item");
                        g_list_foreach ( l->data, ( GFunc ) vfs_file_info_unref, NULL );
                        g_list_free( l->data );
                    }
                }
                g_list_free( file_browser->curhistsel->next );
                file_browser->curhistsel->next = NULL;
            }
            /* Add path to history if there is no forward history */
            file_browser->history = g_list_append( file_browser->history, path );
            file_browser->curHistory = g_list_last( file_browser->history );
            //MOD added - make histsel shadow file_browser->history
            GList* sellist = NULL;
            file_browser->histsel = g_list_append( file_browser->histsel, sellist );
            file_browser->curhistsel = g_list_last( file_browser->histsel );
        }
    }
    else if( mode == PTK_FB_CHDIR_BACK )
    {
        file_browser->curHistory = file_browser->curHistory->prev;
        file_browser->curhistsel = file_browser->curhistsel->prev;
    }
    else if( mode == PTK_FB_CHDIR_FORWARD )
    {
        file_browser->curHistory = file_browser->curHistory->next;
        file_browser->curhistsel = file_browser->curhistsel->next;
    }

    // remove old dir object
    if ( file_browser->dir )
    {
        g_signal_handlers_disconnect_matched( file_browser->dir, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, file_browser );
        g_object_unref( file_browser->dir );
    }

    if ( file_browser->view_mode == PTK_FB_COMPACT_VIEW )
        exo_icon_view_set_model( EXO_ICON_VIEW( folder_view ), NULL );
    else if ( file_browser->view_mode == PTK_FB_LIST_VIEW )
        gtk_tree_view_set_model( GTK_TREE_VIEW( folder_view ), NULL );

    // load new dir
    file_browser->busy = TRUE;
    file_browser->dir = vfs_dir_get_by_path( path );

    if( ! file_browser->curHistory ||
                            path != (char*)file_browser->curHistory->data )
        g_free( path );

    g_signal_emit( file_browser, signals[ BEGIN_CHDIR_SIGNAL ], 0 );

    if( vfs_dir_is_file_listed( file_browser->dir ) )
    {
        on_dir_file_listed( file_browser->dir, FALSE, file_browser );
        file_browser->busy = FALSE;
    }
    else
        file_browser->busy = TRUE;

    g_signal_connect( file_browser->dir, "file-listed", G_CALLBACK( on_dir_file_listed), file_browser );

    ptk_file_browser_update_tab_label( file_browser );

    char* disp_path = g_filename_display_name( ptk_file_browser_get_cwd( file_browser ) );
    if ( !inhibit_focus )
        gtk_entry_set_text( GTK_ENTRY( file_browser->path_bar ), disp_path );

    g_free( disp_path );

    enable_toolbar( file_browser );
    return TRUE;
}

static void on_history_menu_item_activate( GtkWidget* menu_item, PtkFileBrowser* file_browser ) {
    GList* l = (GList*)g_object_get_data( G_OBJECT(menu_item), "path"), *tmp;
    tmp = file_browser->curHistory;
    file_browser->curHistory = l;

    if( !  ptk_file_browser_chdir( file_browser, (char*)l->data, PTK_FB_CHDIR_NO_HISTORY ) )
        file_browser->curHistory = tmp;
    else
    {
        //MOD sync curhistsel
        gint elementn = -1;
        elementn = g_list_position( file_browser->history, file_browser->curHistory );
        if ( elementn != -1 )
            file_browser->curhistsel = g_list_nth( file_browser->histsel, elementn );
        else
            g_debug("missing history item - ptk-file-browser.c");
    }
}

static GtkWidget* add_history_menu_item( PtkFileBrowser* file_browser, GtkWidget* menu, GList* l ) {
    GtkWidget* menu_item, *folder_image;
    char *disp_name;
    disp_name = g_filename_display_basename( (char*)l->data );
    menu_item = gtk_image_menu_item_new_with_label( disp_name );
    g_object_set_data( G_OBJECT( menu_item ), "path", l );
    folder_image = gtk_image_new_from_icon_name( "gnome-fs-directory", GTK_ICON_SIZE_MENU );
    gtk_image_menu_item_set_image ( GTK_IMAGE_MENU_ITEM ( menu_item ), folder_image );
    g_signal_connect( menu_item, "activate", G_CALLBACK( on_history_menu_item_activate ), file_browser );

    gtk_menu_shell_append( GTK_MENU_SHELL(menu), menu_item );
    return menu_item;
}

void ptk_file_browser_show_history_menu( PtkFileBrowser* file_browser, gboolean is_back_history, GdkEventButton* event ) {
    //GtkMenuShell* menu = (GtkMenuShell*)gtk_menu_tool_button_get_menu(btn);
    GtkWidget* menu = gtk_menu_new();
    GList *l;
    gboolean has_items = FALSE;

    if ( is_back_history )
    {
        // back history
        for( l = file_browser->curHistory->prev; l != NULL; l = l->prev )
        {
            add_history_menu_item( file_browser, GTK_WIDGET(menu), l );
            if ( !has_items )
                has_items = TRUE;
        }
    } else {
        // forward history
        for( l = file_browser->curHistory->next; l != NULL; l = l->next )
        {
            add_history_menu_item( file_browser, GTK_WIDGET(menu), l );
            if ( !has_items )
                has_items = TRUE;
        }
    }
    if ( has_items )
    {
        gtk_widget_show_all( GTK_WIDGET( menu ) );
        gtk_menu_popup( GTK_MENU( menu ), NULL, NULL, NULL, NULL, event ? event->button : 0, event ? event->time : 0 );
    }
    else
        gtk_widget_destroy( menu );
}

#if 0
static gboolean   ptk_file_browser_delayed_content_change( PtkFileBrowser* file_browser ) {
    GTimeVal t;
    g_get_current_time( &t );
    file_browser->prev_update_time = t.tv_sec;
    g_signal_emit( file_browser, signals[ CONTENT_CHANGE_SIGNAL ], 0 );
    file_browser->update_timeout = 0;
    return FALSE;
}
#endif

#if 0
void on_folder_content_update ( FolderContent* content, PtkFileBrowser* file_browser ) {
    /*  FIXME: Newly added or deleted files should not be delayed.                       howdy bub
        This must be fixed before 0.2.0 release.  */
    GTimeVal t;
    g_get_current_time( &t );
    /*
      Previous update is < 5 seconds before.
      Queue the update, and don't update the view too often
    */
    if ( ( t.tv_sec - file_browser->prev_update_time ) < 5 )
    {
        /*
          If the update timeout has been set, wait until the timeout happens, and don't do anything here.
        */
        if ( 0 == file_browser->update_timeout )
        { /* No timeout callback. Add one */
            /* Delay the update */
            file_browser->update_timeout = g_timeout_add( 5000, (GSourceFunc)ptk_file_browser_delayed_content_change, file_browser );
        }
    }
    else if ( 0 == file_browser->update_timeout )
    { /* No timeout callback. Add one */
        file_browser->prev_update_time = t.tv_sec;
        g_signal_emit( file_browser, signals[ CONTENT_CHANGE_SIGNAL ], 0 );
    }
}
#endif


static gboolean ptk_file_browser_content_changed( PtkFileBrowser* file_browser ) {
    //gdk_threads_enter();  not needed because g_idle_add runs in main loop thread
    g_signal_emit( file_browser, signals[ CONTENT_CHANGE_SIGNAL ], 0 );
    //gdk_threads_leave();
    return FALSE;
}

static void on_folder_content_changed( VFSDir* dir, VFSFileInfo* file, PtkFileBrowser* file_browser ) {
    if ( file == NULL )
    {
        // The current folder itself changed
        if ( !g_file_test( ptk_file_browser_get_cwd( file_browser ), G_FILE_TEST_IS_DIR ) )
            // current folder doesn't exist - was renamed
            on_close_notebook_page( NULL, file_browser );
    }
    else
        g_idle_add( ( GSourceFunc ) ptk_file_browser_content_changed, file_browser );
}

static void on_file_deleted( VFSDir* dir, VFSFileInfo* file, PtkFileBrowser* file_browser ) {
    if( file == NULL )
    {
        // The folder itself was deleted
        on_close_notebook_page( NULL, file_browser );
        //ptk_file_browser_chdir( file_browser, g_get_home_dir(), PTK_FB_CHDIR_ADD_HISTORY);
    } else {
#if GTK_CHECK_VERSION(3, 0, 0)
#else
        /* GTK2 does not select the next row in the list when a row is deleted, * so do so.  GTK3 does this automatically. */
        GList* sel_files = NULL;
        GtkTreeModel* model;
        GtkTreeSelection* tree_sel = NULL;

        // get model and sel_files if only one selected
        if ( file_browser->view_mode == PTK_FB_COMPACT_VIEW )
        {
            sel_files = folder_view_get_selected_items( file_browser, &model );
            if ( sel_files && sel_files->next )
            {
                // more than on file selected - do nothing
                g_list_foreach( sel_files, ( GFunc ) gtk_tree_path_free, NULL );
                g_list_free( sel_files );
                sel_files = NULL;
            }
        }
        else if ( file_browser->view_mode == PTK_FB_LIST_VIEW )
        {
            tree_sel = gtk_tree_view_get_selection( GTK_TREE_VIEW( file_browser->folder_view ) );
            if ( gtk_tree_selection_count_selected_rows( tree_sel ) == 1 )
                sel_files = gtk_tree_selection_get_selected_rows( tree_sel, &model );
        }

        if ( sel_files )
        {
            VFSFileInfo* file_sel;
            GtkTreeIter it;
            GtkTreeIter it2;
            GtkTreeIter it_prev;
            GtkTreePath* tree_path = NULL;
            if ( gtk_tree_model_get_iter( model, &it, ( GtkTreePath* ) sel_files->data ) )
            {
                gtk_tree_model_get( model, &it, COL_FILE_INFO, &file_sel, -1 );
                if ( file_sel == file )
                {
                    // currently selected file is being deleted, select next
                    if ( gtk_tree_model_iter_next (model, &it ) )
                        tree_path = gtk_tree_model_get_path( model, &it );
                    else if ( gtk_tree_model_get_iter_first( model, &it2 ) )
                    {
                        // file is last in list, select previous
                        it_prev.stamp = 0;
                        do
                        {
                            if ( it2.user_data == it.user_data &&
                                 it2.user_data2 == it.user_data2 &&
                                 it2.user_data3 == it.user_data3 )
                            {
                                // found deleted file
                                if ( it_prev.stamp )
                                {
                                    // there was a previous so select
                                    tree_path = gtk_tree_model_get_path( model, &it_prev );
                                }
                                break;
                            }
                            it_prev = it2;
                        } while ( gtk_tree_model_iter_next (model, &it2 ) );
                    }
                    if ( tree_path )
                    {
                        if ( file_browser->view_mode == PTK_FB_COMPACT_VIEW )
                        {
                            exo_icon_view_select_path( EXO_ICON_VIEW( file_browser->folder_view ), tree_path );
                            exo_icon_view_set_cursor( EXO_ICON_VIEW( file_browser->folder_view ), tree_path, NULL, FALSE );
                            exo_icon_view_scroll_to_path( EXO_ICON_VIEW( file_browser->folder_view ), tree_path, TRUE, .25, 0 );
                        } else {
                            gtk_tree_selection_select_path( tree_sel, tree_path );
                            gtk_tree_view_set_cursor( GTK_TREE_VIEW( file_browser->folder_view ), tree_path, NULL, FALSE );
                            gtk_tree_view_scroll_to_cell( GTK_TREE_VIEW( file_browser->folder_view ), tree_path, NULL, TRUE, .25, 0 );
                        }
                        gtk_tree_path_free( tree_path );
                    }
                }
            }
            g_list_foreach( sel_files, ( GFunc ) gtk_tree_path_free, NULL );
            g_list_free( sel_files );
        }
#endif
        on_folder_content_changed( dir, file, file_browser );
    }
}

static void on_sort_col_changed( GtkTreeSortable* sortable, PtkFileBrowser* file_browser ) {
    int col;

    gtk_tree_sortable_get_sort_column_id( sortable, &col, &file_browser->sort_type );

    switch ( col )
    {
    case COL_FILE_NAME:
        col = PTK_FB_SORT_BY_NAME;
        break;
    case COL_FILE_SIZE:
        col = PTK_FB_SORT_BY_SIZE;
        break;
    case COL_FILE_MTIME:
        col = PTK_FB_SORT_BY_MTIME;
        break;
    case COL_FILE_DESC:
        col = PTK_FB_SORT_BY_TYPE;
        break;
    case COL_FILE_PERM:
        col = PTK_FB_SORT_BY_PERM;
        break;
    case COL_FILE_OWNER:
        col = PTK_FB_SORT_BY_OWNER;
        break;
    }
    file_browser->sort_order = col;
    //MOD enable following to make column click permanent sort
//    app_settings.sort_order = col;
//    if ( file_browser )
//        ptk_file_browser_set_sort_order( PTK_FILE_BROWSER( file_browser ), app_settings.sort_order );

    char* val = g_strdup_printf( "%d", col );
    xset_set_panel( file_browser->mypanel, "list_detailed", "x", val );
    g_free( val );
    val = g_strdup_printf( "%d", file_browser->sort_type );
    xset_set_panel( file_browser->mypanel, "list_detailed", "y", val );
    g_free( val );
}

void ptk_file_browser_update_model( PtkFileBrowser* file_browser ) {
    PtkFileList * list;
    GtkTreeModel *old_list;

    list = ptk_file_list_new( file_browser->dir, file_browser->show_hidden_files );
    old_list = file_browser->file_list;
    file_browser->file_list = GTK_TREE_MODEL( list );
    if ( old_list )
        g_object_unref( G_OBJECT( old_list ) );

    ptk_file_browser_read_sort_extra( file_browser );
    gtk_tree_sortable_set_sort_column_id( GTK_TREE_SORTABLE( list ), file_list_order_from_sort_order( file_browser->sort_order ), file_browser->sort_type );

    show_thumbnails( file_browser, list, file_browser->large_icons, file_browser->max_thumbnail );
    g_signal_connect( list, "sort-column-changed", G_CALLBACK( on_sort_col_changed ), file_browser );

    if ( file_browser->view_mode == PTK_FB_COMPACT_VIEW )
        exo_icon_view_set_model( EXO_ICON_VIEW( file_browser->folder_view ), GTK_TREE_MODEL( list ) );
    else if ( file_browser->view_mode == PTK_FB_LIST_VIEW )
        gtk_tree_view_set_model( GTK_TREE_VIEW( file_browser->folder_view ), GTK_TREE_MODEL( list ) );

// try to smooth list bounce created by delayed re-appearance of column headers
//while( gtk_events_pending() )
//    gtk_main_iteration();

}

void on_dir_file_listed( VFSDir* dir, gboolean is_cancelled, PtkFileBrowser* file_browser ) {
    file_browser->n_sel_files = 0;

    if ( G_LIKELY( ! is_cancelled ) )
    {
        g_signal_connect( dir, "file-created", G_CALLBACK( on_folder_content_changed ), file_browser );
        g_signal_connect( dir, "file-deleted", G_CALLBACK( on_file_deleted ), file_browser );
        g_signal_connect( dir, "file-changed", G_CALLBACK( on_folder_content_changed ), file_browser );
    }

    ptk_file_browser_update_model( file_browser );
    file_browser->busy = FALSE;

    /* Ensuring free space at the end of the heap is freed to the OS,
     * mainly to deal with the possibility that changing the directory results in
     * thousands of large thumbnails being freed, but the memory not actually released  */
#if defined (__GLIBC__)
    malloc_trim(0);
#endif

    g_signal_emit( file_browser, signals[ AFTER_CHDIR_SIGNAL ], 0 );
    //g_signal_emit( file_browser, signals[ CONTENT_CHANGE_SIGNAL ], 0 );
    g_signal_emit( file_browser, signals[ SEL_CHANGE_SIGNAL ], 0 );

    if ( file_browser->side_dir )
        ptk_dir_tree_view_chdir( GTK_TREE_VIEW( file_browser->side_dir ), ptk_file_browser_get_cwd( file_browser ) );

/*
    if ( file_browser->side_pane )
    if ( ptk_file_browser_is_side_pane_visible( file_browser ) )
    {
        side_pane_chdir( file_browser, ptk_file_browser_get_cwd( file_browser ) );
    }
*/
    if ( file_browser->side_dev )
        ptk_location_view_chdir( GTK_TREE_VIEW( file_browser->side_dev ), ptk_file_browser_get_cwd( file_browser ) );
    if ( file_browser->side_book )
        ptk_bookmark_view_chdir( GTK_TREE_VIEW( file_browser->side_book ), file_browser, TRUE );

    //FIXME:  This is already done in update_model, but is there any better way to reduce unnecessary code?
    if ( file_browser->view_mode == PTK_FB_COMPACT_VIEW )
    {   //sfm why is this needed for compact view???
        if ( G_LIKELY(! is_cancelled) && file_browser->file_list )
        {
            show_thumbnails( file_browser, PTK_FILE_LIST( file_browser->file_list ), file_browser->large_icons, file_browser->max_thumbnail );
        }
    }
}

void ptk_file_browser_canon( PtkFileBrowser* file_browser, const char* path ) {
    const char* cwd = ptk_file_browser_get_cwd( file_browser );
    char buf[ PATH_MAX + 1 ];
    char* canon = realpath( path, buf );
    if ( !canon || !g_strcmp0( canon, cwd ) || !g_strcmp0( canon, path ) )
        return;

    if ( g_file_test( canon, G_FILE_TEST_IS_DIR ) )
    {
        // open dir
        ptk_file_browser_chdir( file_browser, canon, PTK_FB_CHDIR_ADD_HISTORY );
        gtk_widget_grab_focus( GTK_WIDGET( file_browser->folder_view ) );
    }
    else if ( g_file_test( canon, G_FILE_TEST_EXISTS ) )
    {
        // open dir and select file
        char* dir_path = g_path_get_dirname( canon );
        if ( dir_path && strcmp( dir_path, cwd ) )
        {
            g_free( file_browser->select_path );
            file_browser->select_path = strdup( canon );
            ptk_file_browser_chdir( file_browser, dir_path, PTK_FB_CHDIR_ADD_HISTORY );
        }
        else
            ptk_file_browser_select_file( file_browser, canon );
        g_free( dir_path );
        gtk_widget_grab_focus( GTK_WIDGET( file_browser->folder_view ) );
    }
}

const char* ptk_file_browser_get_cwd( PtkFileBrowser* file_browser ) {
    if ( ! file_browser->curHistory )
        return NULL;
    return ( const char* ) file_browser->curHistory->data;
}

gboolean ptk_file_browser_is_busy( PtkFileBrowser* file_browser ) {
    return file_browser->busy;
}

gboolean ptk_file_browser_can_back( PtkFileBrowser* file_browser ) {
    /* there is back history */
    return ( file_browser->curHistory && file_browser->curHistory->prev );
}

void ptk_file_browser_go_back( GtkWidget* item, PtkFileBrowser* file_browser ) {
    const char * path;

    focus_folder_view( file_browser );
    /* there is no back history */
    if ( ! file_browser->curHistory || ! file_browser->curHistory->prev )
        return;
    path = ( const char* ) file_browser->curHistory->prev->data;
    ptk_file_browser_chdir( file_browser, path, PTK_FB_CHDIR_BACK );
}

gboolean ptk_file_browser_can_forward( PtkFileBrowser* file_browser ) {
    /* If there is forward history */
    return ( file_browser->curHistory && file_browser->curHistory->next );
}

void ptk_file_browser_go_forward( GtkWidget* item, PtkFileBrowser* file_browser ) {
    const char * path;

    focus_folder_view( file_browser );
    /* If there is no forward history */
    if ( ! file_browser->curHistory || ! file_browser->curHistory->next )
        return ;
    path = ( const char* ) file_browser->curHistory->next->data;
    ptk_file_browser_chdir( file_browser, path, PTK_FB_CHDIR_FORWARD );
}

void ptk_file_browser_go_up( GtkWidget* item, PtkFileBrowser* file_browser ) {
    char * parent_dir;

    focus_folder_view( file_browser );
    parent_dir = g_path_get_dirname( ptk_file_browser_get_cwd( file_browser ) );
    if( strcmp( parent_dir, ptk_file_browser_get_cwd( file_browser ) ) )
        ptk_file_browser_chdir( file_browser, parent_dir, PTK_FB_CHDIR_ADD_HISTORY);
    g_free( parent_dir );
}

void ptk_file_browser_go_home( GtkWidget* item, PtkFileBrowser* file_browser ) {
//    if ( app_settings.home_folder )
//        ptk_file_browser_chdir( PTK_FILE_BROWSER( file_browser ), app_settings.home_folder, PTK_FB_CHDIR_ADD_HISTORY );
//    else
    focus_folder_view( file_browser );
        ptk_file_browser_chdir( PTK_FILE_BROWSER( file_browser ), g_get_home_dir(), PTK_FB_CHDIR_ADD_HISTORY );
}

void ptk_file_browser_go_default( GtkWidget* item, PtkFileBrowser* file_browser ) {
    focus_folder_view( file_browser );
    char* path = xset_get_s( "go_set_default" );
    if ( path && path[0] != '\0' )
        ptk_file_browser_chdir( PTK_FILE_BROWSER( file_browser ), path, PTK_FB_CHDIR_ADD_HISTORY );
    else if ( geteuid() != 0 )
        ptk_file_browser_chdir( PTK_FILE_BROWSER( file_browser ), g_get_home_dir(), PTK_FB_CHDIR_ADD_HISTORY );
    else
        ptk_file_browser_chdir( PTK_FILE_BROWSER( file_browser ), "/", PTK_FB_CHDIR_ADD_HISTORY );
}

void ptk_file_browser_set_default_folder( GtkWidget* item, PtkFileBrowser* file_browser ) {
    xset_set( "go_set_default", "s", ptk_file_browser_get_cwd( file_browser ) );
}

GtkWidget* ptk_file_browser_get_folder_view( PtkFileBrowser* file_browser ) {
    return file_browser->folder_view;
}

/* FIXME: unused function */
GtkTreeView* ptk_file_browser_get_dir_tree( PtkFileBrowser* file_browser ) {
    return NULL;
}

void ptk_file_browser_select_all( GtkWidget* item, PtkFileBrowser* file_browser ) {
    GtkTreeSelection * tree_sel;
    if ( file_browser->view_mode == PTK_FB_COMPACT_VIEW )
    {
        exo_icon_view_select_all( EXO_ICON_VIEW( file_browser->folder_view ) );
    }
    else if ( file_browser->view_mode == PTK_FB_LIST_VIEW )
    {
        tree_sel = gtk_tree_view_get_selection( GTK_TREE_VIEW( file_browser->folder_view ) );
        gtk_tree_selection_select_all( tree_sel );
    }
}

void ptk_file_browser_unselect_all( GtkWidget* item, PtkFileBrowser* file_browser ) {
    GtkTreeSelection * tree_sel;
    if ( file_browser->view_mode == PTK_FB_COMPACT_VIEW )
    {
        exo_icon_view_unselect_all( EXO_ICON_VIEW( file_browser->folder_view ) );
    }
    else if ( file_browser->view_mode == PTK_FB_LIST_VIEW )
    {
        tree_sel = gtk_tree_view_get_selection( GTK_TREE_VIEW( file_browser->folder_view ) );
        gtk_tree_selection_unselect_all( tree_sel );
    }
}

static gboolean   invert_selection ( GtkTreeModel* model, GtkTreePath *path, GtkTreeIter* it, PtkFileBrowser* file_browser ) {
    GtkTreeSelection * tree_sel;
    if ( file_browser->view_mode == PTK_FB_COMPACT_VIEW )
    {
        if ( exo_icon_view_path_is_selected( EXO_ICON_VIEW( file_browser->folder_view ), path ) )
            exo_icon_view_unselect_path( EXO_ICON_VIEW( file_browser->folder_view ), path );
        else
            exo_icon_view_select_path( EXO_ICON_VIEW( file_browser->folder_view ), path );
    }
    else if ( file_browser->view_mode == PTK_FB_LIST_VIEW )
    {
        tree_sel = gtk_tree_view_get_selection( GTK_TREE_VIEW( file_browser->folder_view ) );
        if ( gtk_tree_selection_path_is_selected ( tree_sel, path ) )
            gtk_tree_selection_unselect_path ( tree_sel, path );
        else
            gtk_tree_selection_select_path ( tree_sel, path );
    }
    return FALSE;
}

void ptk_file_browser_invert_selection( GtkWidget* item, PtkFileBrowser* file_browser ) {
    GtkTreeModel * model;
    if ( file_browser->view_mode == PTK_FB_COMPACT_VIEW )
    {
        model = exo_icon_view_get_model( EXO_ICON_VIEW( file_browser->folder_view ) );
        g_signal_handlers_block_matched( file_browser->folder_view, G_SIGNAL_MATCH_FUNC, 0, 0, NULL, on_folder_view_item_sel_change, NULL );
        gtk_tree_model_foreach ( model, ( GtkTreeModelForeachFunc ) invert_selection, file_browser );
        g_signal_handlers_unblock_matched( file_browser->folder_view, G_SIGNAL_MATCH_FUNC, 0, 0, NULL, on_folder_view_item_sel_change, NULL );
        on_folder_view_item_sel_change( EXO_ICON_VIEW( file_browser->folder_view ), file_browser );
    }
    else if ( file_browser->view_mode == PTK_FB_LIST_VIEW )
    {
        GtkTreeSelection* tree_sel;
        tree_sel = gtk_tree_view_get_selection( GTK_TREE_VIEW( file_browser->folder_view ) );
        g_signal_handlers_block_matched( tree_sel, G_SIGNAL_MATCH_FUNC, 0, 0, NULL, on_folder_view_item_sel_change, NULL );
        model = gtk_tree_view_get_model( GTK_TREE_VIEW( file_browser->folder_view ) );
        gtk_tree_model_foreach ( model, ( GtkTreeModelForeachFunc ) invert_selection, file_browser );
        g_signal_handlers_unblock_matched( tree_sel, G_SIGNAL_MATCH_FUNC, 0, 0, NULL, on_folder_view_item_sel_change, NULL );
        on_folder_view_item_sel_change( (ExoIconView*)tree_sel, file_browser );
    }
}

void ptk_file_browser_select_pattern( GtkWidget* item, PtkFileBrowser* file_browser, const char* search_key ) {
    GtkTreeModel* model;
    GtkTreePath* path;
    GtkTreeIter it;
    GtkTreeSelection* tree_sel;
    VFSFileInfo* file;
    gboolean select;
    char* name;
    const char* key;

    if ( search_key )
        key = search_key;
    else
    {
        // get pattern from user  (store in ob1 so it's not saved)
        XSet* set = xset_get( "select_patt" );
        if ( !xset_text_dialog( GTK_WIDGET( file_browser ), _("Select By Pattern"), NULL, FALSE,
                _("Enter pattern to select files and folders:\n\nIf your pattern contains any uppercase characters, the matching will be case sensitive.\n\nExample:  *sp*e?m*\n\nTIP: You can also enter '%% PATTERN' in the path bar."),
                                            NULL, set->ob1, &set->ob1, NULL, FALSE, NULL ) || !set->ob1 )
            return;
        key = set->ob1;
    }

    // case insensitive search ?
    gboolean icase = FALSE;
    char* lower_key = g_utf8_strdown( key, -1 );
    if ( !strcmp( lower_key, key ) )
    {
        // key is all lowercase so do icase search
        icase = TRUE;
    }
    g_free( lower_key );

    // get model, treesel, and stop signals
    if ( file_browser->view_mode == PTK_FB_COMPACT_VIEW )
    {
        model = exo_icon_view_get_model( EXO_ICON_VIEW( file_browser->folder_view ) );
        g_signal_handlers_block_matched( file_browser->folder_view, G_SIGNAL_MATCH_FUNC, 0, 0, NULL, on_folder_view_item_sel_change, NULL );
    }
    else if ( file_browser->view_mode == PTK_FB_LIST_VIEW )
    {
        tree_sel = gtk_tree_view_get_selection( GTK_TREE_VIEW( file_browser->folder_view ) );
        g_signal_handlers_block_matched( tree_sel, G_SIGNAL_MATCH_FUNC, 0, 0, NULL, on_folder_view_item_sel_change, NULL );
        model = gtk_tree_view_get_model( GTK_TREE_VIEW( file_browser->folder_view ) );
    }

    // test rows
    gboolean first_select = TRUE;
    if ( gtk_tree_model_get_iter_first( model, &it ) )
    {
        do
        {
            // get file
            gtk_tree_model_get( model, &it, COL_FILE_INFO, &file, -1 );
            if ( !file )
                continue;

            // test name
            name = (char*)vfs_file_info_get_disp_name( file );
            if ( icase )
                name = g_utf8_strdown( name, -1 );

            select = fnmatch( key, name, 0 ) == 0;

            if ( icase )
                g_free( name );

            // do selection and scroll to first selected
            path = gtk_tree_model_get_path( GTK_TREE_MODEL( PTK_FILE_LIST( file_browser->file_list ) ), &it );
            if ( file_browser->view_mode == PTK_FB_COMPACT_VIEW )
            {
                // select
                if ( exo_icon_view_path_is_selected( EXO_ICON_VIEW( file_browser->folder_view ), path ) )
                {
                    if ( !select )
                        exo_icon_view_unselect_path( EXO_ICON_VIEW( file_browser->folder_view ), path );
                }
                else if ( select )
                    exo_icon_view_select_path( EXO_ICON_VIEW( file_browser->folder_view ), path );

                // scroll to first and set cursor
                if ( first_select && select )
                {
                    exo_icon_view_set_cursor( EXO_ICON_VIEW( file_browser->folder_view ), path, NULL, FALSE );
                    exo_icon_view_scroll_to_path( EXO_ICON_VIEW( file_browser->folder_view ), path, TRUE, .25, 0 );
                    first_select = FALSE;
                }
            }
            else if ( file_browser->view_mode == PTK_FB_LIST_VIEW )
            {
                // select
                if ( gtk_tree_selection_path_is_selected ( tree_sel, path ) )
                {
                    if ( !select )
                        gtk_tree_selection_unselect_path( tree_sel, path );
                }
                else if ( select )
                    gtk_tree_selection_select_path( tree_sel, path );

                // scroll to first and set cursor
                if ( first_select && select )
                {
                    gtk_tree_view_set_cursor( GTK_TREE_VIEW( file_browser->folder_view ), path, NULL, FALSE);
                    gtk_tree_view_scroll_to_cell( GTK_TREE_VIEW( file_browser->folder_view ), path, NULL, TRUE, .25, 0 );
                    first_select = FALSE;
                }
            }
            gtk_tree_path_free( path );
        }
        while ( gtk_tree_model_iter_next( model, &it ) );
    }

    // restore signals and trigger sel change
    if ( file_browser->view_mode == PTK_FB_COMPACT_VIEW )
    {
        g_signal_handlers_unblock_matched( file_browser->folder_view, G_SIGNAL_MATCH_FUNC, 0, 0, NULL, on_folder_view_item_sel_change, NULL );
        on_folder_view_item_sel_change( EXO_ICON_VIEW( file_browser->folder_view ), file_browser );
    }
    else if ( file_browser->view_mode == PTK_FB_LIST_VIEW )
    {
        g_signal_handlers_unblock_matched( tree_sel, G_SIGNAL_MATCH_FUNC, 0, 0, NULL, on_folder_view_item_sel_change, NULL );
        on_folder_view_item_sel_change( (ExoIconView*)tree_sel, file_browser );
    }
    focus_folder_view( file_browser );
}

void ptk_file_browser_select_file_list( PtkFileBrowser* file_browser, char** filename, gboolean do_select ) {
    // If do_select, select all filenames, unselect others
    // if !do_select, unselect filenames, leave others unchanged
    // If !*filename select or unselect all
    GtkTreeModel* model;
    GtkTreePath* path;
    GtkTreeIter it;
    GtkTreeSelection* tree_sel;
    VFSFileInfo* file;
    gboolean select;
    char* name;
    char** test_name;

    if ( !filename || ! *filename )
    {
        if ( do_select )
            ptk_file_browser_select_all( NULL, file_browser );
        else
            ptk_file_browser_unselect_all( NULL, file_browser );
        return;
    }

    // get model, treesel, and stop signals
    if ( file_browser->view_mode == PTK_FB_COMPACT_VIEW )
    {
        model = exo_icon_view_get_model( EXO_ICON_VIEW( file_browser->folder_view ) );
        g_signal_handlers_block_matched( file_browser->folder_view, G_SIGNAL_MATCH_FUNC, 0, 0, NULL, on_folder_view_item_sel_change, NULL );
    }
    else if ( file_browser->view_mode == PTK_FB_LIST_VIEW )
    {
        tree_sel = gtk_tree_view_get_selection( GTK_TREE_VIEW( file_browser->folder_view ));
        g_signal_handlers_block_matched( tree_sel, G_SIGNAL_MATCH_FUNC, 0, 0, NULL, on_folder_view_item_sel_change, NULL );
        model = gtk_tree_view_get_model( GTK_TREE_VIEW( file_browser->folder_view ) );
    }

    // test rows
    gboolean first_select = TRUE;
    if ( gtk_tree_model_get_iter_first( model, &it ) )
    {
        do
        {
            // get file
            gtk_tree_model_get( model, &it, COL_FILE_INFO, &file, -1 );
            if ( !file )
                continue;

            // test name
            name = (char*)vfs_file_info_get_disp_name( file );
            test_name = filename;
            while ( *test_name )
            {
                if ( !strcmp( *test_name, name ) )
                    break;
                test_name++;
            }
            if ( *test_name )
                select = do_select;
            else
                select = !do_select;

            // do selection and scroll to first selected
            path = gtk_tree_model_get_path( GTK_TREE_MODEL( PTK_FILE_LIST( file_browser->file_list ) ), &it );
            if ( file_browser->view_mode == PTK_FB_COMPACT_VIEW )
            {
                // select
                if ( exo_icon_view_path_is_selected( EXO_ICON_VIEW( file_browser->folder_view ), path ) )
                {
                    if ( !select )
                        exo_icon_view_unselect_path( EXO_ICON_VIEW( file_browser->folder_view ), path );
                }
                else if ( select && do_select )
                    exo_icon_view_select_path( EXO_ICON_VIEW( file_browser->folder_view ), path );

                // scroll to first and set cursor
                if ( first_select && select && do_select )
                {
                    exo_icon_view_set_cursor( EXO_ICON_VIEW( file_browser->folder_view ), path, NULL, FALSE );
                    exo_icon_view_scroll_to_path( EXO_ICON_VIEW( file_browser->folder_view ), path, TRUE, .25, 0 );
                    first_select = FALSE;
                }
            }
            else if ( file_browser->view_mode == PTK_FB_LIST_VIEW )
            {
                // select
                if ( gtk_tree_selection_path_is_selected ( tree_sel, path ) )
                {
                    if ( !select )
                        gtk_tree_selection_unselect_path( tree_sel, path );
                }
                else if ( select && do_select )
                    gtk_tree_selection_select_path( tree_sel, path );

                // scroll to first and set cursor
                if ( first_select && select && do_select )
                {
                    gtk_tree_view_set_cursor( GTK_TREE_VIEW( file_browser->folder_view ), path, NULL, FALSE);
                    gtk_tree_view_scroll_to_cell( GTK_TREE_VIEW( file_browser->folder_view ), path, NULL, TRUE, .25, 0 );
                    first_select = FALSE;
                }
            }
            gtk_tree_path_free( path );
        }
        while ( gtk_tree_model_iter_next( model, &it ) );
    }

    // restore signals and trigger sel change
    if ( file_browser->view_mode == PTK_FB_COMPACT_VIEW )
    {
        g_signal_handlers_unblock_matched( file_browser->folder_view, G_SIGNAL_MATCH_FUNC, 0, 0, NULL, on_folder_view_item_sel_change, NULL );
        on_folder_view_item_sel_change( EXO_ICON_VIEW( file_browser->folder_view ), file_browser );
    }
    else if ( file_browser->view_mode == PTK_FB_LIST_VIEW )
    {
        g_signal_handlers_unblock_matched( tree_sel, G_SIGNAL_MATCH_FUNC, 0, 0, NULL, on_folder_view_item_sel_change, NULL );
        on_folder_view_item_sel_change( (ExoIconView*)tree_sel, file_browser );
    }
    focus_folder_view( file_browser );
}

void ptk_file_browser_seek_path( PtkFileBrowser* file_browser, const char* seek_dir, const char* seek_name ) {
    // change to dir seek_dir if needed; select first dir or else file with
    // prefix seek_name
    const char* cwd = ptk_file_browser_get_cwd( file_browser );

    if ( seek_dir && g_strcmp0( cwd, seek_dir ) )
    {
        // change dir
        g_free( file_browser->seek_name );
        file_browser->seek_name = g_strdup( seek_name );
        file_browser->inhibit_focus = TRUE;
        if ( !ptk_file_browser_chdir( file_browser, seek_dir, PTK_FB_CHDIR_ADD_HISTORY ) )
        {
            file_browser->inhibit_focus = FALSE;
            g_free( file_browser->seek_name );
            file_browser->seek_name = NULL;
        }
        // return here to allow dir to load
        // finishes seek in main-window.c on_file_browser_after_chdir()
        return;
    }

    // no change dir was needed or was called from on_file_browser_after_chdir()
    // select seek name
    ptk_file_browser_unselect_all( NULL, file_browser );

    if ( !( seek_name && seek_name[0] ) )
        return;

    // get model, treesel, and stop signals
    GtkTreeModel* model;
    GtkTreePath* path;
    GtkTreeIter it;
    GtkTreeIter it_file;
    GtkTreeIter it_dir;
    it_file.stamp = 0;
    it_dir.stamp = 0;
    GtkTreeSelection* tree_sel;
    VFSFileInfo* file;
    gboolean select;
    char* name;
    if ( file_browser->view_mode == PTK_FB_COMPACT_VIEW )
    {
        model = exo_icon_view_get_model( EXO_ICON_VIEW( file_browser->folder_view ) );
        g_signal_handlers_block_matched( file_browser->folder_view, G_SIGNAL_MATCH_FUNC, 0, 0, NULL, on_folder_view_item_sel_change, NULL );
    }
    else if ( file_browser->view_mode == PTK_FB_LIST_VIEW )
    {
        tree_sel = gtk_tree_view_get_selection( GTK_TREE_VIEW( file_browser->folder_view ) );
        g_signal_handlers_block_matched( tree_sel, G_SIGNAL_MATCH_FUNC, 0, 0, NULL, on_folder_view_item_sel_change, NULL );
        model = gtk_tree_view_get_model( GTK_TREE_VIEW( file_browser->folder_view ) );
    }
    if ( !GTK_IS_TREE_MODEL( model ) )
        goto _restore_sig;

    // test rows - give preference to matching dir, else match file
    if ( gtk_tree_model_get_iter_first( model, &it ) )
    {
        do
        {
            // get file
            gtk_tree_model_get( model, &it, COL_FILE_INFO, &file, -1 );
            if ( !file )
                continue;

            // test name
            name = (char*)vfs_file_info_get_disp_name( file );
            if ( !g_strcmp0( name, seek_name ) )
            {
                // exact match (may be file or dir)
                it_dir = it;
                break;
            }
            if ( g_str_has_prefix( name, seek_name ) )
            {
                // prefix found
                if ( vfs_file_info_is_dir( file ) )
                {
                    if ( !it_dir.stamp )
                        it_dir = it;
                }
                else if ( !it_file.stamp )
                    it_file = it;
            }
        }
        while ( gtk_tree_model_iter_next( model, &it ) );
    }

    if ( it_dir.stamp )
        it = it_dir;
    else
        it = it_file;
    if ( !it.stamp )
        goto _restore_sig;

    // do selection and scroll to selected
    path = gtk_tree_model_get_path( GTK_TREE_MODEL( PTK_FILE_LIST( file_browser->file_list ) ), &it );
    if ( !path )
        goto _restore_sig;
    if ( file_browser->view_mode == PTK_FB_COMPACT_VIEW )
    {
        // select
        exo_icon_view_select_path( EXO_ICON_VIEW( file_browser->folder_view ), path );

        // scroll and set cursor
        exo_icon_view_set_cursor( EXO_ICON_VIEW( file_browser->folder_view ), path, NULL, FALSE );
        exo_icon_view_scroll_to_path( EXO_ICON_VIEW( file_browser->folder_view ), path, TRUE, .25, 0 );
    }
    else if ( file_browser->view_mode == PTK_FB_LIST_VIEW )
    {
        // select
        gtk_tree_selection_select_path( tree_sel, path );

        // scroll and set cursor
        gtk_tree_view_set_cursor(GTK_TREE_VIEW( file_browser->folder_view ), path, NULL, FALSE);
        gtk_tree_view_scroll_to_cell( GTK_TREE_VIEW( file_browser->folder_view ), path, NULL, TRUE, .25, 0 );
    }
    gtk_tree_path_free( path );

_restore_sig:
    // restore signals and trigger sel change
    if ( file_browser->view_mode == PTK_FB_COMPACT_VIEW )
    {
        g_signal_handlers_unblock_matched( file_browser->folder_view, G_SIGNAL_MATCH_FUNC, 0, 0, NULL, on_folder_view_item_sel_change, NULL );
        on_folder_view_item_sel_change( EXO_ICON_VIEW( file_browser->folder_view ), file_browser );
    }
    else if ( file_browser->view_mode == PTK_FB_LIST_VIEW )
    {
        g_signal_handlers_unblock_matched( tree_sel, G_SIGNAL_MATCH_FUNC, 0, 0, NULL, on_folder_view_item_sel_change, NULL );
        on_folder_view_item_sel_change( (ExoIconView*)tree_sel, file_browser );
    }
}

/* signal handlers */

void   on_folder_view_item_activated ( ExoIconView *iconview, GtkTreePath *path, PtkFileBrowser* file_browser ) {
    ptk_file_browser_open_selected_files( file_browser );
}

void   on_folder_view_row_activated ( GtkTreeView *tree_view, GtkTreePath *path, GtkTreeViewColumn* col, PtkFileBrowser* file_browser ) {
    //file_browser->button_press = FALSE;
    ptk_file_browser_open_selected_files( file_browser );
}

gboolean on_folder_view_item_sel_change_idle( PtkFileBrowser* file_browser ) {
    GList * sel_files;
    GList* sel;
    GtkTreeIter it;
    GtkTreeModel* model;
    VFSFileInfo* file;

    if ( !GTK_IS_WIDGET( file_browser ) )
        return FALSE;

    file_browser->n_sel_files = 0;
    file_browser->sel_size = 0;

    sel_files = folder_view_get_selected_items( file_browser, &model );

    for ( sel = sel_files; sel; sel = g_list_next( sel ) )
    {
        if ( gtk_tree_model_get_iter( model, &it, ( GtkTreePath* ) sel->data ) )
        {
            gtk_tree_model_get( model, &it, COL_FILE_INFO, &file, -1 );
            if ( file )
            {
                file_browser->sel_size += vfs_file_info_get_size( file );
                vfs_file_info_unref( file );
            }
            ++file_browser->n_sel_files;
        }
    }

    g_list_foreach( sel_files, ( GFunc ) gtk_tree_path_free, NULL );
    g_list_free( sel_files );

    g_signal_emit( file_browser, signals[ SEL_CHANGE_SIGNAL ], 0 );
    file_browser->sel_change_idle = 0;
    return FALSE;
}

void on_folder_view_item_sel_change( ExoIconView *iconview, PtkFileBrowser* file_browser ) {
    /* //sfm on_folder_view_item_sel_change fires for each selected file
     * when a file is clicked - causes hang if thousands of files are selected
     * So add only one g_idle_add at a time
     */
    if ( file_browser->sel_change_idle )
        return;

    file_browser->sel_change_idle = g_idle_add( (GSourceFunc)on_folder_view_item_sel_change_idle, file_browser );
}

static void show_popup_menu( PtkFileBrowser* file_browser, GdkEventButton *event ) {
    const char * cwd;
    char* dir_name = NULL;
    guint32 time;
    gint button;
    GtkWidget* popup;
    char* file_path = NULL;
    VFSFileInfo* file;
    GList* sel_files;

    cwd = ptk_file_browser_get_cwd( file_browser );
    sel_files = ptk_file_browser_get_selected_files( file_browser );
    if( ! sel_files )
    {
        file = NULL;
/*
        file = vfs_file_info_new();
        vfs_file_info_get( file, cwd, NULL );
        sel_files = g_list_prepend( NULL, vfs_file_info_ref( file ) );
        file_path = g_strdup( cwd );
*/        /* dir_name = g_path_get_dirname( cwd ); */
    } else {
        file = vfs_file_info_ref( (VFSFileInfo*)sel_files->data );
        file_path = g_build_filename( cwd, vfs_file_info_get_name( file ), NULL );
    }
    //MOD added G_FILE_TEST_IS_SYMLINK for dangling symlink popup menu
//    if ( g_file_test( file_path, G_FILE_TEST_EXISTS | G_FILE_TEST_IS_SYMLINK ) )
//    {
        if ( event )
        {
            button = event->button;
            time = event->time;
        } else {
            button = 0;
            time = gtk_get_current_event_time();
        }
        popup = ptk_file_menu_new( NULL, file_browser, file_path, file, dir_name ? dir_name : cwd, sel_files );
        if ( popup )
            gtk_menu_popup( GTK_MENU( popup ), NULL, NULL, NULL, NULL, button, time );
//    }
//    else if ( sel_files )
//    {
//        vfs_file_info_list_free( sel_files );
//    }
    if ( file )
        vfs_file_info_unref( file );

    if ( file_path )
        g_free( file_path );
    if ( dir_name )
        g_free( dir_name );
}

/* invoke popup menu via shortcut key */
gboolean   on_folder_view_popup_menu ( GtkWidget* widget, PtkFileBrowser* file_browser ) {
    show_popup_menu( file_browser, NULL );
    return TRUE;
}

gboolean   on_folder_view_button_press_event ( GtkWidget *widget, GdkEventButton *event, PtkFileBrowser* file_browser ) {
    VFSFileInfo * file;
    GtkTreeModel * model = NULL;
    GtkTreePath *tree_path = NULL;
    GtkTreeViewColumn* col = NULL;
    GtkTreeIter it;
    gchar *file_path;
    GtkTreeSelection* tree_sel;
    gboolean ret = FALSE;

    if ( file_browser->menu_shown )
        file_browser->menu_shown = FALSE;

    if ( event->type == GDK_BUTTON_PRESS )
    {
        focus_folder_view( file_browser );
        //file_browser->button_press = TRUE;

        if ( ( evt_win_click->s || evt_win_click->ob2_data ) &&
                main_window_event( file_browser->main_window, evt_win_click, "evt_win_click", 0, 0, "filelist", 0, event->button, event->state, TRUE ) )
        {
            file_browser->skip_release = TRUE;
            return TRUE;
        }

        if ( event->button == 4 || event->button == 5 ||
             event->button == 8 || event->button == 9 )
        {
            if ( event->button == 4 || event->button == 8 )
                ptk_file_browser_go_back( NULL, file_browser );
            else
                ptk_file_browser_go_forward( NULL, file_browser );
            return TRUE;
        }

        // Alt - Left/Right Click
        if ( ( ( event->state &
                        ( GDK_SHIFT_MASK | GDK_CONTROL_MASK | GDK_MOD1_MASK ) )
                        == GDK_MOD1_MASK ) &&
                        ( event->button == 1 || event->button == 3 ) )
        {
            if ( event->button == 1 )
                ptk_file_browser_go_back( NULL, file_browser );
            else
                ptk_file_browser_go_forward( NULL, file_browser );
            return TRUE;
        }

        if ( file_browser->view_mode == PTK_FB_COMPACT_VIEW )
        {
            tree_path = exo_icon_view_get_path_at_pos( EXO_ICON_VIEW( widget ), event->x, event->y );
            model = exo_icon_view_get_model( EXO_ICON_VIEW( widget ) );

            /* deselect selected files when right click on blank area */
            if ( !tree_path && event->button == 3 )
                exo_icon_view_unselect_all ( EXO_ICON_VIEW( widget ) );
        }
        else if ( file_browser->view_mode == PTK_FB_LIST_VIEW )
        {
            model = gtk_tree_view_get_model( GTK_TREE_VIEW( widget ) );
            gtk_tree_view_get_path_at_pos( GTK_TREE_VIEW( widget ), event->x, event->y, &tree_path, &col, NULL, NULL );
            tree_sel = gtk_tree_view_get_selection( GTK_TREE_VIEW( widget ) );

            if( col && gtk_tree_view_column_get_sort_column_id(col) !=
                                            COL_FILE_NAME && tree_path )
            {
                gtk_tree_path_free( tree_path );
                tree_path = NULL;
            }
        }

        /* an item is clicked, get its file path */
        if ( tree_path && gtk_tree_model_get_iter( model, &it, tree_path ) )
        {
            gtk_tree_model_get( model, &it, COL_FILE_INFO, &file, -1 );
            file_path = g_build_filename( ptk_file_browser_get_cwd( file_browser ), vfs_file_info_get_name( file ), NULL );
        }
        else /* no item is clicked */
        {
            file = NULL;
            file_path = NULL;
        }

        /* middle button */
        if ( event->button == 2 && file_path ) /* middle click on a item */
        {
            /* open in new tab if its a folder */
            if ( G_LIKELY( file_path ) )
            {
                if ( g_file_test( file_path, G_FILE_TEST_IS_DIR ) )
                {
                    g_signal_emit( file_browser, signals[ OPEN_ITEM_SIGNAL ], 0, file_path, PTK_OPEN_NEW_TAB );
                }
            }
            ret = TRUE;
        }
        else if ( event->button == 3 ) /* right click */
        {
            /* cancel all selection, and select the item if it's not selected */
            if ( file_browser->view_mode == PTK_FB_COMPACT_VIEW )
            {
                if ( tree_path &&
                    !exo_icon_view_path_is_selected ( EXO_ICON_VIEW( widget ), tree_path ) )
                {
                    exo_icon_view_unselect_all ( EXO_ICON_VIEW( widget ) );
                    exo_icon_view_select_path( EXO_ICON_VIEW( widget ), tree_path );
                }
            }
            else if( file_browser->view_mode == PTK_FB_LIST_VIEW )
            {
                if ( tree_path &&
                    !gtk_tree_selection_path_is_selected( tree_sel, tree_path ) )
                {
                    gtk_tree_selection_unselect_all( tree_sel );
                    gtk_tree_selection_select_path( tree_sel, tree_path );
                }
            }
            show_popup_menu( file_browser, event );
            /* FIXME if approx 5000 are selected, right-click sometimes unselects all
             * after this button_press function returns - why?  a gtk or exo bug?
             * Always happens with above show_popup_menu call disabled
             * Only when this occurs, cursor is automatically set to current row and
             * treesel 'changed' signal fires
             * Stopping changed signal had no effect
             * Using connect rather than connect_after had no effect
             * Removing signal connect had no effect
             * FIX: inhibit button release */
            ret = file_browser->menu_shown = TRUE;
        }
        if ( file )
            vfs_file_info_unref( file );
        g_free( file_path );
        gtk_tree_path_free( tree_path );
    }
    else if ( event->type == GDK_2BUTTON_PRESS && event->button == 1 )
    {
        // double click event -  button = 0
        if ( ( evt_win_click->s || evt_win_click->ob2_data ) &&
                main_window_event( file_browser->main_window, evt_win_click, "evt_win_click", 0, 0, "filelist", 0, 0, event->state, TRUE ) )
            return TRUE;

        if ( file_browser->view_mode == PTK_FB_LIST_VIEW )
            /* set ret TRUE to prevent drag_begin starting in this tab after
             * fuseiso mount.  Why?
             * row_activated occurs before GDK_2BUTTON_PRESS so use
             * file_browser->button_press to determine if row was already
             * activated or user clicked on non-row */
            ret = TRUE;
        else if ( !app_settings.single_click )
            /* sfm 1.0.6 set skip_release for Icon/Compact to prevent file
             * under cursor being selected when entering dir with double-click.
             * Also see conditional reset of skip_release in
             * ptk_file_browser_chdir(). See also
             * on_folder_view_button_release_event() */
            file_browser->skip_release = TRUE;
    }
/*  go up if double-click in blank area of file list - this was disabled due
 * to complaints about accidental clicking
    else if ( file_browser->button_press && event->type == GDK_2BUTTON_PRESS
                                                        && event->button == 1 )
    {
        if ( file_browser->view_mode == PTK_FB_COMPACT_VIEW )
        {
            tree_path = exo_icon_view_get_path_at_pos( EXO_ICON_VIEW( widget ), event->x, event->y );
            if ( !tree_path )
            {
                ptk_file_browser_go_up( NULL, file_browser );
                ret = TRUE;
            }
            else
                gtk_tree_path_free( tree_path );
        }
        else if( file_browser->view_mode == PTK_FB_LIST_VIEW )
        {
            // row_activated occurs before GDK_2BUTTON_PRESS so use
            // file_browser->button_press to determine if row was already activated
            // or user clicked on non-row
            ptk_file_browser_go_up( NULL, file_browser );
            ret = TRUE;
        }
    }
*/
    return ret;
}

gboolean   on_folder_view_button_release_event ( GtkWidget *widget, GdkEventButton *event, PtkFileBrowser* file_browser )
{   // on left-click release on file, if not dnd or rubberbanding, unselect files
    GtkTreeModel* model;
    GtkTreePath* tree_path = NULL;
    GtkTreeSelection* tree_sel;

    if ( file_browser->is_drag || event->button != 1 || file_browser->skip_release ||
            ( event->state & ( GDK_SHIFT_MASK | GDK_CONTROL_MASK | GDK_MOD1_MASK ) ) )
    {
        if ( file_browser->skip_release )
            file_browser->skip_release = FALSE;
        // this fixes bug where right-click shows menu and release unselects files
        gboolean ret = file_browser->menu_shown && event->button != 1;
        if ( file_browser->menu_shown )
            file_browser->menu_shown = FALSE;
        return ret;
    }

    if ( file_browser->view_mode == PTK_FB_COMPACT_VIEW )
    {
        if ( exo_icon_view_is_rubber_banding_active( EXO_ICON_VIEW( widget ) ) )
            return FALSE;
        /* Conditional on single_click below was removed 1.0.2 708f0988 bc it
         * caused a left-click to not unselect other files.  However, this
         * caused file under cursor to be selected when entering directory by
         * double-click in Icon/Compact styles.  To correct this, 1.0.6
         * conditionally sets skip_release on GDK_2BUTTON_PRESS, and doesn't
         * reset skip_release in ptk_file_browser_chdir(). */
        //if ( app_settings.single_click )
        //{
            tree_path = exo_icon_view_get_path_at_pos( EXO_ICON_VIEW( widget ), event->x, event->y );
            model = exo_icon_view_get_model( EXO_ICON_VIEW( widget ) );
            if ( tree_path )
            {
                // unselect all but one file
                exo_icon_view_unselect_all( EXO_ICON_VIEW( widget ) );
                exo_icon_view_select_path( EXO_ICON_VIEW( widget ), tree_path );
            }
        //}
    }
    else if ( file_browser->view_mode == PTK_FB_LIST_VIEW )
    {
        if ( gtk_tree_view_is_rubber_banding_active( GTK_TREE_VIEW( widget ) ) )
            return FALSE;
        if ( app_settings.single_click )
        {
            model = gtk_tree_view_get_model( GTK_TREE_VIEW( widget ) );
            gtk_tree_view_get_path_at_pos( GTK_TREE_VIEW( widget ), event->x, event->y, &tree_path, NULL, NULL, NULL );
            tree_sel = gtk_tree_view_get_selection( GTK_TREE_VIEW( widget ) );

            if ( tree_path && tree_sel && gtk_tree_selection_count_selected_rows( tree_sel ) > 1 )
            {
                // unselect all but one file
                gtk_tree_selection_unselect_all( tree_sel );
                gtk_tree_selection_select_path( tree_sel, tree_path );
            }
        }
    }
    gtk_tree_path_free( tree_path );
    return FALSE;
}

static gboolean on_dir_tree_update_sel ( PtkFileBrowser* file_browser ) {
    char * dir_path;

    if ( !file_browser->side_dir )
        return FALSE;
    dir_path = ptk_dir_tree_view_get_selected_dir( GTK_TREE_VIEW( file_browser->side_dir ) );

    if ( dir_path )
    {
        if ( strcmp( dir_path, ptk_file_browser_get_cwd( file_browser ) ) )
        {
            gdk_threads_enter(); // needed for gtk_dialog_run in ptk_show_error
            if ( ptk_file_browser_chdir( file_browser, dir_path, PTK_FB_CHDIR_ADD_HISTORY ) )
                gtk_entry_set_text( GTK_ENTRY( file_browser->path_bar ), dir_path );
            gdk_threads_leave();
        }
        g_free( dir_path );
    }
    return FALSE;
}

void on_dir_tree_row_activated ( GtkTreeView* view, GtkTreePath* path, GtkTreeViewColumn* column, PtkFileBrowser* file_browser ) {
    g_idle_add( ( GSourceFunc ) on_dir_tree_update_sel, file_browser );
}

void ptk_file_browser_new_tab( GtkMenuItem* item, PtkFileBrowser* file_browser ) {
    const char* dir_path;

    focus_folder_view( file_browser );
    if ( xset_get_s( "go_set_default" ) )
        dir_path = xset_get_s( "go_set_default" );
    else
        dir_path = g_get_home_dir();

    if ( !g_file_test( dir_path, G_FILE_TEST_IS_DIR ) )
        g_signal_emit( file_browser, signals[ OPEN_ITEM_SIGNAL ], 0, "/", PTK_OPEN_NEW_TAB );
    else
    {
        g_signal_emit( file_browser, signals[ OPEN_ITEM_SIGNAL ], 0, dir_path, PTK_OPEN_NEW_TAB );
    }
}

void ptk_file_browser_new_tab_here( GtkMenuItem* item, PtkFileBrowser* file_browser ) {
    const char* dir_path;

    focus_folder_view( file_browser );
    dir_path = ptk_file_browser_get_cwd( file_browser );
    if ( !g_file_test( dir_path, G_FILE_TEST_IS_DIR ) )
    {
        if ( xset_get_s( "go_set_default" ) )
            dir_path = xset_get_s( "go_set_default" );
        else
            dir_path = g_get_home_dir();
    }
    if ( !g_file_test( dir_path, G_FILE_TEST_IS_DIR ) )
        g_signal_emit( file_browser, signals[ OPEN_ITEM_SIGNAL ], 0, "/", PTK_OPEN_NEW_TAB );
    else
    {
        g_signal_emit( file_browser, signals[ OPEN_ITEM_SIGNAL ], 0, dir_path, PTK_OPEN_NEW_TAB );
    }
}

void ptk_file_browser_save_column_widths( GtkTreeView *view, PtkFileBrowser* file_browser ) {
    const char* title;
    XSet* set = NULL;
    int i, j, width;
    GtkTreeViewColumn* col;

    if ( !( GTK_IS_WIDGET( file_browser ) && GTK_IS_TREE_VIEW( view ) ) )
        return;

    if ( file_browser->view_mode != PTK_FB_LIST_VIEW )
        return;

    FMMainWindow* main_window = (FMMainWindow*)file_browser->main_window;

    // if the window was opened maximized and stayed maximized, or the window is unmaximized and not fullscreen, save the columns
    if ( ( !main_window->maximized || main_window->opened_maximized ) &&  !main_window->fullscreen )
    {
        int p = file_browser->mypanel;
        char mode = main_window->panel_context[p-1];
//printf("*** save_columns  fb=%#x (panel %d)  mode = %d\n", file_browser, p, mode);
        for ( i = 0; i < 7; i++ )
        {
            col = gtk_tree_view_get_column( view, i );
            if ( !col )
                return;
            title = gtk_tree_view_column_get_title( col );
            for ( j = 0; j < 7; j++ )                      ////////////////// howdy    WAS 6
            {
                if ( !strcmp( title, _(column_titles[j]) ) )
                    break;
            }
            if ( j != 7 )
            {
                // save column width for this panel context
                set = xset_get_panel_mode( p, column_names[j], mode );
                width = gtk_tree_view_column_get_width( col );
                if ( width > 0 )
                {
                    g_free( set->y );
                    set->y = g_strdup_printf( "%d", width );
//printf("        %d\t%s\n", width, title );
                }
            }
        }
    }
}

void on_folder_view_columns_changed( GtkTreeView *view, PtkFileBrowser* file_browser ) {
    // user dragged a column to a different position - save positions
    const char* title;
    XSet* set = NULL;
    int i, j, width;
    GtkTreeViewColumn* col;

    if ( !( GTK_IS_WIDGET( file_browser ) && GTK_IS_TREE_VIEW( view ) ) )
        return;

    if ( file_browser->view_mode != PTK_FB_LIST_VIEW )
        return;

    for ( i = 0; i < 6; i++ )
    {
        col = gtk_tree_view_get_column( view, i );
        if ( !col )
            return;
        title = gtk_tree_view_column_get_title( col );
        for ( j = 0; j < 6; j++ )
        {
            if ( !strcmp( title, _(column_titles[j]) ) )
                break;
        }
        if ( j != 6 )
        {
            // save column position
            set = xset_get_panel( file_browser->mypanel, column_names[j] );
            g_free( set->x );
            set->x = g_strdup_printf( "%d", i );
        }
    }
}

void on_folder_view_destroy( GtkTreeView *view, PtkFileBrowser* file_browser ) {
    guint id = g_signal_lookup ("columns-changed", G_TYPE_FROM_INSTANCE(view) );
    if ( id ) {
        gulong hand = g_signal_handler_find( ( gpointer ) view, G_SIGNAL_MATCH_ID, id, 0, NULL, NULL, NULL );
        if ( hand )
            g_signal_handler_disconnect( ( gpointer ) view, hand );
    }
}

gboolean folder_view_search_equal( GtkTreeModel* model, gint col, const gchar* key, GtkTreeIter* it, gpointer search_data ) {
    char* name;
    char* lower_name = NULL;
    char* lower_key;
    gboolean no_match;

    if ( col != COL_FILE_NAME )
        return TRUE;

    gtk_tree_model_get( model, it, col, &name, -1 );

    if ( !name || !key )
        return TRUE;

    lower_key = g_utf8_strdown( key, -1 );
    if ( !strcmp( lower_key, key ) )
    {
        // key is all lowercase so do icase search
        lower_name = g_utf8_strdown( name, -1 );
        name = lower_name;
    }

    if ( strchr( key, '*' ) || strchr( key, '?' ) )
    {
        char* key2 = g_strdup_printf( "*%s*", key );
        no_match = fnmatch( key2, name, 0 ) != 0;
        g_free( key2 );
    } else {
        gboolean end = g_str_has_suffix( key, "$" );
        gboolean start = !end && ( strlen( key ) < 3 );
        char* key2 = g_strdup( key );
        char* keyp = key2;
        if ( key[0] == '^' )
        {
            keyp++;
            start = TRUE;
        }
        if ( end )
            key2[strlen( key2 )-1] = '\0';
        if ( start && end )
            no_match = !strstr( name, keyp );
        else if (start )
            no_match = !g_str_has_prefix( name, keyp );
        else if ( end )
            no_match = !g_str_has_suffix( name, keyp );
        else
            no_match = !strstr( name, key );
        g_free( key2 );
    }
    g_free( lower_name );
    g_free( lower_key );
    return no_match;  //return FALSE for match
}

static GtkWidget* create_folder_view( PtkFileBrowser* file_browser, PtkFBViewMode view_mode ) {
    GtkWidget * folder_view = NULL;
    GtkTreeSelection* tree_sel;
    GtkCellRenderer* renderer;
    int big_icon_size, small_icon_size, icon_size = 0;

    vfs_mime_type_get_icon_size( &big_icon_size, &small_icon_size );

    switch ( view_mode )
    {
    case PTK_FB_COMPACT_VIEW:
        folder_view = exo_icon_view_new();

        if( view_mode == PTK_FB_COMPACT_VIEW )
        {
            icon_size = file_browser->large_icons ? big_icon_size :
                                                    small_icon_size;

            exo_icon_view_set_layout_mode( (ExoIconView*)folder_view, EXO_ICON_VIEW_LAYOUT_COLS );
            exo_icon_view_set_orientation( (ExoIconView*)folder_view, GTK_ORIENTATION_HORIZONTAL );
        } else {
            icon_size = big_icon_size;

            exo_icon_view_set_column_spacing( (ExoIconView*)folder_view, 4 );
            exo_icon_view_set_item_width ( (ExoIconView*)folder_view, icon_size < 110 ? 110 :
                                                icon_size );
        }

        exo_icon_view_set_selection_mode ( (ExoIconView*)folder_view, GTK_SELECTION_MULTIPLE );

        exo_icon_view_set_pixbuf_column ( (ExoIconView*)folder_view, COL_FILE_BIG_ICON );
        exo_icon_view_set_text_column ( (ExoIconView*)folder_view, COL_FILE_NAME );

        // search
        exo_icon_view_set_enable_search( (ExoIconView*)folder_view, TRUE );
        exo_icon_view_set_search_column( (ExoIconView*)folder_view, COL_FILE_NAME );
        exo_icon_view_set_search_equal_func( (ExoIconView*)folder_view, folder_view_search_equal, NULL, NULL );

        exo_icon_view_set_single_click( (ExoIconView*)folder_view, file_browser->single_click );
        exo_icon_view_set_single_click_timeout( (ExoIconView*)folder_view, app_settings.no_single_hover ? 0 : SINGLE_CLICK_TIMEOUT );

        gtk_cell_layout_clear ( GTK_CELL_LAYOUT ( folder_view ) );

        /* renderer = gtk_cell_renderer_pixbuf_new (); */
        file_browser->icon_render = renderer = ptk_file_icon_renderer_new();

        /* add the icon renderer */
        g_object_set ( G_OBJECT ( renderer ), "follow_state", TRUE, NULL );
        gtk_cell_layout_pack_start ( GTK_CELL_LAYOUT ( folder_view ), renderer, FALSE );
        gtk_cell_layout_add_attribute ( GTK_CELL_LAYOUT ( folder_view ), renderer, "pixbuf", file_browser->large_icons ?
                                    COL_FILE_BIG_ICON : COL_FILE_SMALL_ICON );
        gtk_cell_layout_add_attribute ( GTK_CELL_LAYOUT ( folder_view ), renderer, "info", COL_FILE_INFO );
        /* add the name renderer */
        renderer = ptk_text_renderer_new ();

        if( view_mode == PTK_FB_COMPACT_VIEW )
        {
            g_object_set ( G_OBJECT ( renderer ), "xalign", 0.0, "yalign", 0.5, NULL );
        } else {
            g_object_set ( G_OBJECT ( renderer ), "wrap-mode", PANGO_WRAP_WORD_CHAR, "wrap-width", icon_size < 110 ? 109 :
                            icon_size, "xalign", 0.5, "yalign", 0.0, NULL );
        }
        gtk_cell_layout_pack_start ( GTK_CELL_LAYOUT ( folder_view ), renderer, TRUE );
        gtk_cell_layout_add_attribute ( GTK_CELL_LAYOUT ( folder_view ), renderer, "text", COL_FILE_NAME );

        exo_icon_view_enable_model_drag_source ( EXO_ICON_VIEW( folder_view ), ( GDK_CONTROL_MASK | GDK_BUTTON1_MASK | GDK_BUTTON3_MASK ), drag_targets, G_N_ELEMENTS( drag_targets ), GDK_ACTION_ALL );

        exo_icon_view_enable_model_drag_dest ( EXO_ICON_VIEW( folder_view ), drag_targets, G_N_ELEMENTS( drag_targets ), GDK_ACTION_ALL );

        g_signal_connect ( ( gpointer ) folder_view, "item-activated", G_CALLBACK ( on_folder_view_item_activated ), file_browser );

        g_signal_connect_after ( ( gpointer ) folder_view, "selection-changed", G_CALLBACK ( on_folder_view_item_sel_change ), file_browser );

        break;
    case PTK_FB_LIST_VIEW:
        folder_view = exo_tree_view_new ();

        init_list_view( file_browser, GTK_TREE_VIEW( folder_view ) );

        tree_sel = gtk_tree_view_get_selection( GTK_TREE_VIEW( folder_view ) );
        gtk_tree_selection_set_mode( tree_sel, GTK_SELECTION_MULTIPLE );

        if ( xset_get_b( "rubberband" ) )
            gtk_tree_view_set_rubber_banding( (GtkTreeView*)folder_view, TRUE );

        // Search
        gtk_tree_view_set_enable_search( (GtkTreeView*)folder_view, TRUE );
        gtk_tree_view_set_search_column( (GtkTreeView*)folder_view, COL_FILE_NAME );
        gtk_tree_view_set_search_equal_func( (GtkTreeView*)folder_view, folder_view_search_equal, NULL, NULL );

        exo_tree_view_set_single_click( (ExoTreeView*)folder_view, file_browser->single_click );
        exo_tree_view_set_single_click_timeout( (ExoTreeView*)folder_view, app_settings.no_single_hover ? 0 : SINGLE_CLICK_TIMEOUT );

        icon_size = file_browser->large_icons ? big_icon_size : small_icon_size;

        gtk_tree_view_enable_model_drag_source ( GTK_TREE_VIEW( folder_view ), ( GDK_CONTROL_MASK | GDK_BUTTON1_MASK | GDK_BUTTON3_MASK ), drag_targets, G_N_ELEMENTS( drag_targets ), GDK_ACTION_ALL );

        gtk_tree_view_enable_model_drag_dest ( GTK_TREE_VIEW( folder_view ), drag_targets, G_N_ELEMENTS( drag_targets ), GDK_ACTION_ALL );

        g_signal_connect ( ( gpointer ) folder_view, "row_activated", G_CALLBACK ( on_folder_view_row_activated ), file_browser );

        g_signal_connect_after ( ( gpointer ) tree_sel, "changed", G_CALLBACK ( on_folder_view_item_sel_change ), file_browser );

        g_signal_connect ( ( gpointer ) folder_view, "columns-changed", G_CALLBACK ( on_folder_view_columns_changed ), file_browser );
        g_signal_connect ( ( gpointer ) folder_view, "destroy", G_CALLBACK ( on_folder_view_destroy ), file_browser );
        break;
    }

    gtk_cell_renderer_set_fixed_size( file_browser->icon_render, icon_size, icon_size );

    g_signal_connect ( ( gpointer ) folder_view, "button-press-event", G_CALLBACK ( on_folder_view_button_press_event ), file_browser );
    g_signal_connect ( ( gpointer ) folder_view, "button-release-event", G_CALLBACK ( on_folder_view_button_release_event ), file_browser );

    //g_signal_connect ( ( gpointer ) folder_view,
    //                   "key_press_event",
    //                   G_CALLBACK ( on_folder_view_key_press_event ),
    //                   file_browser );

    g_signal_connect ( ( gpointer ) folder_view, "popup-menu", G_CALLBACK ( on_folder_view_popup_menu ), file_browser );

    /* init drag & drop support */

    g_signal_connect ( ( gpointer ) folder_view, "drag-data-received", G_CALLBACK ( on_folder_view_drag_data_received ), file_browser );

    g_signal_connect ( ( gpointer ) folder_view, "drag-data-get", G_CALLBACK ( on_folder_view_drag_data_get ), file_browser );

    g_signal_connect ( ( gpointer ) folder_view, "drag-begin", G_CALLBACK ( on_folder_view_drag_begin ), file_browser );

    g_signal_connect ( ( gpointer ) folder_view, "drag-motion", G_CALLBACK ( on_folder_view_drag_motion ), file_browser );

    g_signal_connect ( ( gpointer ) folder_view, "drag-leave", G_CALLBACK ( on_folder_view_drag_leave ), file_browser );

    g_signal_connect ( ( gpointer ) folder_view, "drag-drop", G_CALLBACK ( on_folder_view_drag_drop ), file_browser );

    g_signal_connect ( ( gpointer ) folder_view, "drag-end", G_CALLBACK ( on_folder_view_drag_end ), file_browser );

    // set font
    if ( xset_get_s_panel( file_browser->mypanel, "font_file" ) )
    {
        PangoFontDescription* font_desc = pango_font_description_from_string( xset_get_s_panel( file_browser->mypanel, "font_file" ) );
        gtk_widget_modify_font( folder_view, font_desc );
        pango_font_description_free( font_desc );
    }

    return folder_view;
}


void init_list_view( PtkFileBrowser* file_browser, GtkTreeView* list_view ) {
    GtkTreeViewColumn * col;
    GtkCellRenderer *renderer;
    GtkCellRenderer *pix_renderer;
    int i, j, width;
    XSet* set;

    int cols[] = { COL_FILE_NAME, COL_FILE_SIZE, COL_FILE_DESC, COL_FILE_PERM, COL_FILE_OWNER, COL_FILE_MTIME };

    FMMainWindow* main_window = (FMMainWindow*)file_browser->main_window;
    int p = file_browser->mypanel;
    char mode = main_window->panel_context[p-1];

    for ( i = 0; i < G_N_ELEMENTS( cols ); i++ )
    {
        col = gtk_tree_view_column_new ();
        gtk_tree_view_column_set_resizable ( col, TRUE );

        renderer = gtk_cell_renderer_text_new();

        // column order
        for ( j = 0; j < G_N_ELEMENTS( cols ); j++ )
        {
            if ( xset_get_int_panel( p, column_names[j], "x" ) == i )
                break;
        }
        if ( j == G_N_ELEMENTS( cols ) )
            j = i; // failsafe
        else
        {
            // column width
            gtk_tree_view_column_set_min_width( col, 50 );
            gtk_tree_view_column_set_sizing( col, GTK_TREE_VIEW_COLUMN_FIXED );
            set = xset_get_panel_mode( p, column_names[j], mode );
            width = set->y ? atoi( set->y ) : 100;
            if ( width )
            {
                if ( cols[j] == COL_FILE_NAME && !app_settings.always_show_tabs
                                && file_browser->view_mode == PTK_FB_LIST_VIEW
                                && gtk_notebook_get_n_pages( GTK_NOTEBOOK( file_browser->mynotebook ) ) == 1 )
                {
                    // when tabs are added, the width of the notebook decreases by a few pixels,
                    // meaning there is not enough space for all columns - this causes a horizontal scrollbar to
                    // appear on new and sometimes first tab, so shave some pixels off first columns                     howdy
                    gtk_tree_view_column_set_fixed_width ( col, width - 6 );

                    // below causes increasing reduction of column every time new tab is added and closed - undesirable
                    PtkFileBrowser* first_fb = (PtkFileBrowser*) gtk_notebook_get_nth_page( GTK_NOTEBOOK( file_browser->mynotebook ), 0 );

                    if ( first_fb && first_fb->view_mode == PTK_FB_LIST_VIEW && GTK_IS_TREE_VIEW( first_fb->folder_view ) )
                    {
                        GtkTreeViewColumn* first_col = gtk_tree_view_get_column( GTK_TREE_VIEW( first_fb->folder_view ), 0 );
                        if ( first_col )
                        {
                            int first_width = gtk_tree_view_column_get_width( first_col );
                            if ( first_width > 10 )
                                gtk_tree_view_column_set_fixed_width( first_col, first_width - 6 );
                        }
                    }
                } else {
                    gtk_tree_view_column_set_fixed_width ( col, width );
                    //printf("init set_width %s %d\n", column_names[j], width );
                }
            }
        }

        if ( cols[j] == COL_FILE_NAME )
        {
            g_object_set( G_OBJECT( renderer ), /* "editable", TRUE, */    "ellipsize", PANGO_ELLIPSIZE_END, NULL );
            /*
            g_signal_connect( renderer, "editing-started", G_CALLBACK( on_filename_editing_started ), NULL );
            */
            file_browser->icon_render = pix_renderer = ptk_file_icon_renderer_new();

            gtk_tree_view_column_pack_start( col, pix_renderer, FALSE );
            gtk_tree_view_column_set_attributes( col, pix_renderer, "pixbuf", file_browser->large_icons ? COL_FILE_BIG_ICON : COL_FILE_SMALL_ICON, "info", COL_FILE_INFO, NULL );

            gtk_tree_view_column_set_expand ( col, TRUE );
            gtk_tree_view_column_set_sizing( col, GTK_TREE_VIEW_COLUMN_FIXED );
            gtk_tree_view_column_set_min_width( col, 140 );    //   howdy   WAS 150
            gtk_tree_view_column_set_reorderable( col, FALSE );
            exo_tree_view_set_activable_column( (ExoTreeView*)list_view, col );
        } else {
            gtk_tree_view_column_set_reorderable( col, TRUE );
            gtk_tree_view_column_set_visible( col, xset_get_b_panel_mode( p, column_names[j], mode ) );
        }

        if ( cols[j] == COL_FILE_SIZE )
            gtk_cell_renderer_set_alignment( renderer, 1, 0.5 );        //  xalign, yalign

        gtk_tree_view_column_pack_start( col, renderer, TRUE );
        gtk_tree_view_column_set_attributes( col, renderer, "text", cols[ j ], NULL );
        gtk_tree_view_append_column ( list_view, col );
        gtk_tree_view_column_set_title( col, _( column_titles[ j ] ) );
        gtk_tree_view_column_set_sort_indicator( col, TRUE );
        gtk_tree_view_column_set_sort_column_id( col, cols[ j ] );
        gtk_tree_view_column_set_sort_order( col, GTK_SORT_DESCENDING );
    }
    gtk_tree_view_set_rules_hint ( list_view, TRUE );
}


void ptk_file_browser_refresh( GtkWidget* item, PtkFileBrowser* file_browser ) {
    if ( file_browser->busy )
        // a dir is already loading
        return;

    if ( !g_file_test( ptk_file_browser_get_cwd( file_browser ), G_FILE_TEST_IS_DIR ) )
    {
        on_close_notebook_page( NULL, file_browser );
        return;
    }

    // save cursor's file path for later re-selection
    GtkTreePath* tree_path = NULL;
    GtkTreeModel* model = NULL;
    GtkTreeIter it;
    VFSFileInfo* file;
    char* cursor_path = NULL;
    if ( file_browser->view_mode == PTK_FB_LIST_VIEW )
    {
        gtk_tree_view_get_cursor( GTK_TREE_VIEW( file_browser->folder_view ), &tree_path, NULL );
        model = gtk_tree_view_get_model( GTK_TREE_VIEW( file_browser->folder_view ) );
    }
    else if ( file_browser->view_mode == PTK_FB_COMPACT_VIEW )
    {
        exo_icon_view_get_cursor( EXO_ICON_VIEW( file_browser->folder_view ), &tree_path, NULL );
        model = exo_icon_view_get_model( EXO_ICON_VIEW( file_browser->folder_view ) );
    }
    if ( tree_path && model &&  gtk_tree_model_get_iter( model, &it, tree_path ) )
    {
        gtk_tree_model_get( model, &it, COL_FILE_INFO, &file, -1 );
        if ( file )
        {
            cursor_path = g_build_filename( ptk_file_browser_get_cwd( file_browser ), vfs_file_info_get_name( file ), NULL );
        }
    }
    gtk_tree_path_free( tree_path );

    // these steps are similar to chdir
    // remove old dir object
    if ( file_browser->dir )
    {
        g_signal_handlers_disconnect_matched( file_browser->dir, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, file_browser );
        g_object_unref( file_browser->dir );
        file_browser->dir = NULL;
    }

    // destroy file list and create new one
    ptk_file_browser_update_model( file_browser );

    /* Ensuring free space at the end of the heap is freed to the OS,
     * mainly to deal with the possibility thousands of large thumbnails
     * have been freed but the memory not actually released by zzzFM */
#if defined (__GLIBC__)
    malloc_trim(0);
#endif

    // begin load dir
    file_browser->busy = TRUE;
    file_browser->dir = vfs_dir_get_by_path( ptk_file_browser_get_cwd( file_browser ) );
    g_signal_emit( file_browser, signals[ BEGIN_CHDIR_SIGNAL ], 0 );
    if ( vfs_dir_is_file_listed( file_browser->dir ) )
    {
        on_dir_file_listed( file_browser->dir, FALSE, file_browser );
        if ( cursor_path )
            ptk_file_browser_select_file( file_browser, cursor_path );
        file_browser->busy = FALSE;
    } else {
        file_browser->busy = TRUE;
        g_free( file_browser->select_path );
        file_browser->select_path = g_strdup( cursor_path );
    }
    g_signal_connect( file_browser->dir, "file-listed", G_CALLBACK(on_dir_file_listed), file_browser );

    g_free( cursor_path );
}

guint ptk_file_browser_get_n_all_files( PtkFileBrowser* file_browser ) {
    return file_browser->dir ? file_browser->dir->n_files : 0;
}

guint ptk_file_browser_get_n_visible_files( PtkFileBrowser* file_browser ) {
    return file_browser->file_list ?
           gtk_tree_model_iter_n_children( file_browser->file_list, NULL ) : 0;
}

GList* folder_view_get_selected_items( PtkFileBrowser* file_browser, GtkTreeModel** model ) {
    GtkTreeSelection * tree_sel;
    if ( file_browser->view_mode == PTK_FB_COMPACT_VIEW )
    {
        *model = exo_icon_view_get_model( EXO_ICON_VIEW( file_browser->folder_view ) );
        return exo_icon_view_get_selected_items( EXO_ICON_VIEW( file_browser->folder_view ) );
    }
    else if ( file_browser->view_mode == PTK_FB_LIST_VIEW )
    {
        tree_sel = gtk_tree_view_get_selection( GTK_TREE_VIEW( file_browser->folder_view ) );
        return gtk_tree_selection_get_selected_rows( tree_sel, model );
    }
    return NULL;
}

static char* folder_view_get_drop_dir( PtkFileBrowser* file_browser, int x, int y ) {
    GtkTreePath * tree_path = NULL;
    GtkTreeModel *model = NULL;
    GtkTreeViewColumn* col;
    GtkTreeIter it;
    VFSFileInfo* file;
    char* dest_path = NULL;

    if ( file_browser->view_mode == PTK_FB_COMPACT_VIEW )
    {
        exo_icon_view_widget_to_icon_coords ( EXO_ICON_VIEW( file_browser->folder_view ), x, y, &x, &y );
        tree_path = folder_view_get_tree_path_at_pos( file_browser, x, y );
        model = exo_icon_view_get_model( EXO_ICON_VIEW( file_browser->folder_view ) );
    }
    else if ( file_browser->view_mode == PTK_FB_LIST_VIEW )
    {
        // if drag is in progress, get the dest row path
        gtk_tree_view_get_drag_dest_row( GTK_TREE_VIEW( file_browser->folder_view ), &tree_path, NULL );
        if ( !tree_path )
        {
            // no drag in progress, get drop path
            gtk_tree_view_get_path_at_pos( GTK_TREE_VIEW( file_browser->folder_view ), x, y, NULL, &col, NULL, NULL );
            if ( col == gtk_tree_view_get_column( GTK_TREE_VIEW( file_browser->folder_view ), 0 ) )
            {
                gtk_tree_view_get_dest_row_at_pos( GTK_TREE_VIEW( file_browser->folder_view ), x, y, &tree_path, NULL );
                model = gtk_tree_view_get_model( GTK_TREE_VIEW( file_browser->folder_view ) );
            }
        }
        else
            model = gtk_tree_view_get_model( GTK_TREE_VIEW( file_browser->folder_view ) );
    }
    if ( tree_path )
    {
        if ( G_UNLIKELY( ! gtk_tree_model_get_iter( model, &it, tree_path ) ) )
            return NULL;

        gtk_tree_model_get( model, &it, COL_FILE_INFO, &file, -1 );
        if ( file )
        {
            if ( vfs_file_info_is_dir( file ) )
            {
                dest_path = g_build_filename( ptk_file_browser_get_cwd( file_browser ), vfs_file_info_get_name( file ), NULL );
/*  this isn't needed?
                // dest_path is a link? resolve
                if ( g_file_test( dest_path, G_FILE_TEST_IS_SYMLINK ) )
                {
                    char* old_dest = dest_path;
                    dest_path = g_file_read_link( old_dest, NULL );
                    g_free( old_dest );
                }
*/
            }
            else  /* Drop on a file, not folder */
            {
                /* Return current directory */
                dest_path = g_strdup( ptk_file_browser_get_cwd( file_browser ) );
            }
            vfs_file_info_unref( file );
        }
        gtk_tree_path_free( tree_path );
    } else {
        dest_path = g_strdup( ptk_file_browser_get_cwd( file_browser ) );
    }
    return dest_path;
}


void on_folder_view_drag_data_received ( GtkWidget *widget, GdkDragContext *drag_context, gint x, gint y,
                                         GtkSelectionData *sel_data, guint info, guint time, gpointer user_data ) {
    gchar **list, **puri;
    GList* files = NULL;
    PtkFileTask* task;
    VFSFileTaskType file_action = VFS_FILE_TASK_MOVE;
    PtkFileBrowser* file_browser = ( PtkFileBrowser* ) user_data;
    char* dest_dir;
    char* file_path;
    GtkWidget* parent_win;
    /*  Don't call the default handler  */
    g_signal_stop_emission_by_name( widget, "drag-data-received" );

    if ( ( gtk_selection_data_get_length(sel_data) >= 0 ) &&
                            ( gtk_selection_data_get_format(sel_data) == 8 ) )
    {
        // (list view) use stored x and y because == 0 for update drag status
        //             when is last row (gtk2&3 bug?)
        // and because exo_icon_view has no get_drag_dest_row
        dest_dir = folder_view_get_drop_dir( file_browser, file_browser->drag_x, file_browser->drag_y );
//printf("FB dest_dir = %s\n", dest_dir );
        if ( dest_dir )
        {
            puri = list = gtk_selection_data_get_uris( sel_data );

            if( file_browser->pending_drag_status )
            {
                // We only want to update drag status, not really want to drop
                dev_t dest_dev;
                ino_t dest_inode;
                struct stat statbuf;    // skip stat64
                if( stat( dest_dir, &statbuf ) == 0 )
                {
                    dest_dev = statbuf.st_dev;
                    dest_inode = statbuf.st_ino;
                    if( 0 == file_browser->drag_source_dev )
                    {
                        file_browser->drag_source_dev = dest_dev;
                        for( ; *puri; ++puri )
                        {
                            file_path = g_filename_from_uri( *puri, NULL, NULL );
                            if( file_path && stat( file_path, &statbuf ) == 0 )
                            {
                                if ( statbuf.st_dev != dest_dev )
                                {
                                    // different devices - store source device
                                    file_browser->drag_source_dev = statbuf.st_dev;
                                    g_free( file_path );
                                    break;
                                }
                                else if ( file_browser->drag_source_inode == 0 )
                                {
                                    // same device - store source parent inode
                                    char* src_dir = g_path_get_dirname( file_path );
                                    if ( src_dir && stat( src_dir, &statbuf ) == 0 )
                                    {
                                        file_browser->drag_source_inode =
                                                                statbuf.st_ino;
                                    }
                                    g_free( src_dir );
                                }
                            }
                            g_free( file_path );
                        }
                    }
                    if ( file_browser->drag_source_dev != dest_dev ||
                           file_browser->drag_source_inode == dest_inode )
                        // src and dest are on different devices or same dir
                        gdk_drag_status (drag_context, GDK_ACTION_COPY, time);
                    else
                        gdk_drag_status (drag_context, GDK_ACTION_MOVE, time);
                }
                else
                    // stat failed
                    gdk_drag_status (drag_context, GDK_ACTION_COPY, time);

                g_free( dest_dir );
                g_strfreev( list );
                file_browser->pending_drag_status = 0;
                return;
            }
            if ( puri )
            {
                if ( 0 == ( gdk_drag_context_get_selected_action(drag_context) &  ( GDK_ACTION_MOVE | GDK_ACTION_COPY | GDK_ACTION_LINK ) ) )
                {
                    gdk_drag_status (drag_context, GDK_ACTION_COPY, time); //sfm correct?  was MOVE
                }
                gtk_drag_finish ( drag_context, TRUE, FALSE, time );

                while ( *puri )
                {
                    if ( **puri == '/' )
                        file_path = g_strdup( *puri );
                    else
                        file_path = g_filename_from_uri( *puri, NULL, NULL );

                    if ( file_path )
                        files = g_list_prepend( files, file_path );
                    ++puri;
                }
                g_strfreev( list );

                switch ( gdk_drag_context_get_selected_action(drag_context) )
                {
                case GDK_ACTION_COPY:
                    file_action = VFS_FILE_TASK_COPY;
                    break;
                case GDK_ACTION_LINK:
                    file_action = VFS_FILE_TASK_LINK;
                    break;
                    /* FIXME:
                      GDK_ACTION_DEFAULT, GDK_ACTION_PRIVATE, and GDK_ACTION_ASK are not handled */
                default:
                    break;
                }

                if ( files )
                {
                    /* g_print( "dest_dir = %s\n", dest_dir ); */

                    /* We only want to update drag status, not really want to drop */
                    if( file_browser->pending_drag_status )
                    {
                        struct stat statbuf;    // skip stat64
                        if( stat( dest_dir, &statbuf ) == 0 )
                        {
                            file_browser->pending_drag_status = 0;

                        }
                        g_list_foreach( files, (GFunc)g_free, NULL );
                        g_list_free( files );
                        g_free( dest_dir );
                        return;
                    }
                    else /* Accept the drop and perform file actions */
                    {
                        parent_win = gtk_widget_get_toplevel( GTK_WIDGET( file_browser ) );
                        task = ptk_file_task_new( file_action, files, dest_dir, GTK_WINDOW( parent_win ), file_browser->task_view );
                        ptk_file_task_run( task );
                    }
                }
                g_free( dest_dir );
                gtk_drag_finish ( drag_context, TRUE, FALSE, time );
                return ;
            }
        }
        g_free( dest_dir );
    }

    /* If we are only getting drag status, not finished. */
    if( file_browser->pending_drag_status )
    {
        file_browser->pending_drag_status = 0;
        return;
    }
    gtk_drag_finish ( drag_context, FALSE, FALSE, time );
}


void on_folder_view_drag_data_get ( GtkWidget *widget, GdkDragContext *drag_context, GtkSelectionData *sel_data, guint info, guint time, PtkFileBrowser *file_browser ) {
    GdkAtom type = gdk_atom_intern( "text/uri-list", FALSE );
    gchar* uri;
    GString* uri_list = g_string_sized_new( 8192 );
    GList* sels = ptk_file_browser_get_selected_files( file_browser );
    GList* sel;
    VFSFileInfo* file;
    char* full_path;

    /*  Don't call the default handler  */
    g_signal_stop_emission_by_name( widget, "drag-data-get" );

    // drag_context->suggested_action = GDK_ACTION_MOVE;

    for ( sel = sels; sel; sel = g_list_next( sel ) )
    {
        file = ( VFSFileInfo* ) sel->data;
        full_path = g_build_filename( ptk_file_browser_get_cwd( file_browser ), vfs_file_info_get_name( file ), NULL );
        uri = g_filename_to_uri( full_path, NULL, NULL );
        g_free( full_path );
        g_string_append( uri_list, uri );
        g_free( uri );

        g_string_append( uri_list, "\r\n" );
    }
    g_list_foreach( sels, ( GFunc ) vfs_file_info_unref, NULL );
    g_list_free( sels );
    gtk_selection_data_set ( sel_data, type, 8, ( guchar* ) uri_list->str, uri_list->len + 1 );
    g_string_free( uri_list, TRUE );
}


void on_folder_view_drag_begin ( GtkWidget *widget, GdkDragContext *drag_context, PtkFileBrowser* file_browser ) {
    /*  Don't call the default handler  */
    g_signal_stop_emission_by_name ( widget, "drag-begin" );
    /* gtk_drag_set_icon_stock ( drag_context, GTK_STOCK_DND_MULTIPLE, 1, 1 ); */
    gtk_drag_set_icon_default( drag_context );
    file_browser->is_drag = TRUE;
}

static GtkTreePath*
folder_view_get_tree_path_at_pos( PtkFileBrowser* file_browser, int x, int y ) {
    GtkTreePath *tree_path;

    if ( file_browser->view_mode == PTK_FB_COMPACT_VIEW )
    {
        tree_path = exo_icon_view_get_path_at_pos( EXO_ICON_VIEW( file_browser->folder_view ), x, y );
    }
    else if ( file_browser->view_mode == PTK_FB_LIST_VIEW )
    {
        gtk_tree_view_get_path_at_pos( GTK_TREE_VIEW( file_browser->folder_view ), x, y, &tree_path, NULL, NULL, NULL );
    }
    return tree_path;
}

gboolean on_folder_view_auto_scroll( GtkScrolledWindow* scroll ) {
    GtkAdjustment * vadj;
    gdouble vpos;

    //gdk_threads_enter();   //sfm why is this here?

    vadj = gtk_scrolled_window_get_vadjustment( scroll ) ;
    vpos = gtk_adjustment_get_value( vadj );

    if ( folder_view_auto_scroll_direction == GTK_DIR_UP )
    {
        vpos -= gtk_adjustment_get_step_increment ( vadj );
        if ( vpos > gtk_adjustment_get_lower ( vadj ) )
            gtk_adjustment_set_value ( vadj, vpos );
        else
            gtk_adjustment_set_value ( vadj, gtk_adjustment_get_lower ( vadj ) );
    } else {
        vpos += gtk_adjustment_get_step_increment ( vadj );
        if ( ( vpos + gtk_adjustment_get_page_size ( vadj ) ) <  gtk_adjustment_get_upper ( vadj ) )
            gtk_adjustment_set_value ( vadj, vpos );
        else
            gtk_adjustment_set_value ( vadj, ( gtk_adjustment_get_upper ( vadj ) -  gtk_adjustment_get_page_size ( vadj ) ) );
    }

    //gdk_threads_leave();
    return TRUE;
}


gboolean on_folder_view_drag_motion ( GtkWidget *widget, GdkDragContext *drag_context, gint x, gint y, guint time, PtkFileBrowser* file_browser ) {
    GtkScrolledWindow * scroll;
    GtkAdjustment *vadj;
    gdouble vpos;
    GtkTreeModel* model = NULL;
    GtkTreePath *tree_path;
    GtkTreeViewColumn* col;
    GtkTreeIter it;
    VFSFileInfo* file;
    GdkDragAction suggested_action;
    GdkAtom target;
    GtkTargetList* target_list;
    GtkAllocation allocation;

    /*  Don't call the default handler  */
    g_signal_stop_emission_by_name ( widget, "drag-motion" );

    scroll = GTK_SCROLLED_WINDOW( gtk_widget_get_parent ( widget ) );

    vadj = gtk_scrolled_window_get_vadjustment( scroll ) ;
    vpos = gtk_adjustment_get_value( vadj );
    gtk_widget_get_allocation( widget, &allocation );

    if ( y < 32 )
    {
        /* Auto scroll up */
        if ( ! folder_view_auto_scroll_timer )
        {
            folder_view_auto_scroll_direction = GTK_DIR_UP;
            folder_view_auto_scroll_timer = g_timeout_add( 150, ( GSourceFunc ) on_folder_view_auto_scroll, scroll );
        }
    }
    else if ( y > ( allocation.height - 32 ) )
    {
        if ( ! folder_view_auto_scroll_timer )
        {
            folder_view_auto_scroll_direction = GTK_DIR_DOWN;
            folder_view_auto_scroll_timer = g_timeout_add( 150, ( GSourceFunc ) on_folder_view_auto_scroll, scroll );
        }
    }
    else if ( folder_view_auto_scroll_timer )
    {
        g_source_remove( folder_view_auto_scroll_timer );
        folder_view_auto_scroll_timer = 0;
    }

    tree_path = NULL;
    if ( file_browser->view_mode == PTK_FB_COMPACT_VIEW )
    {
        // store x and y because exo_icon_view has no get_drag_dest_row
        file_browser->drag_x = x;
        file_browser->drag_y = y;
        exo_icon_view_widget_to_icon_coords( EXO_ICON_VIEW( widget ), x, y, &x, &y );
        tree_path = exo_icon_view_get_path_at_pos( EXO_ICON_VIEW( widget ), x, y );
        model = exo_icon_view_get_model( EXO_ICON_VIEW( widget ) );
    } else {
        // store x and y because == 0 for update drag status when is last row
        file_browser->drag_x = x;
        file_browser->drag_y = y;
        if ( gtk_tree_view_get_path_at_pos( GTK_TREE_VIEW( widget ), x, y, NULL, &col, NULL, NULL ) )
        {
            if ( gtk_tree_view_get_column ( GTK_TREE_VIEW( widget ), 0 ) == col )
            {
                gtk_tree_view_get_dest_row_at_pos ( GTK_TREE_VIEW( widget ), x, y, &tree_path, NULL );
                model = gtk_tree_view_get_model( GTK_TREE_VIEW( widget ) );
            }
        }
    }

    if ( tree_path )
    {
        if ( gtk_tree_model_get_iter( model, &it, tree_path ) )
        {
            gtk_tree_model_get( model, &it, COL_FILE_INFO, &file, -1 );
            if ( ! file || ! vfs_file_info_is_dir( file ) )
            {
                gtk_tree_path_free( tree_path );
                tree_path = NULL;
            }
            vfs_file_info_unref( file );
        }
    }

    if ( file_browser->view_mode == PTK_FB_COMPACT_VIEW )
    {
        exo_icon_view_set_drag_dest_item ( EXO_ICON_VIEW( widget ), tree_path, EXO_ICON_VIEW_DROP_INTO );
    }
    else if ( file_browser->view_mode == PTK_FB_LIST_VIEW )
    {
        gtk_tree_view_set_drag_dest_row( GTK_TREE_VIEW( widget ), tree_path, GTK_TREE_VIEW_DROP_INTO_OR_AFTER );
    }

    if ( tree_path )
        gtk_tree_path_free( tree_path );

    /* FIXME: Creating a new target list everytime is very inefficient,
         but currently gtk_drag_dest_get_target_list always returns NULL
         due to some strange reason, and cannot be used currently.  */
    target_list = gtk_target_list_new( drag_targets, G_N_ELEMENTS(drag_targets) );
    target = gtk_drag_dest_find_target( widget, drag_context, target_list );
    gtk_target_list_unref( target_list );

    if (target == GDK_NONE)
        gdk_drag_status( drag_context, 0, time);
    else
    {
        /* Only 'move' is available. The user force move action by pressing Shift key */
        if( ( gdk_drag_context_get_actions ( drag_context ) & GDK_ACTION_ALL)  == GDK_ACTION_MOVE )
            suggested_action = GDK_ACTION_MOVE;
        /* Only 'copy' is available. The user force copy action by pressing Ctrl key */
        else if( (gdk_drag_context_get_actions ( drag_context ) & GDK_ACTION_ALL)  == GDK_ACTION_COPY )
            suggested_action = GDK_ACTION_COPY;
        /* Only 'link' is available. The user force link action by pressing Shift+Ctrl key */
        else if( (gdk_drag_context_get_actions ( drag_context ) & GDK_ACTION_ALL)  == GDK_ACTION_LINK )
            suggested_action = GDK_ACTION_LINK;
        /* Several different actions are available. We have to figure out a good default action. */
        else
        {
            int drag_action = xset_get_int( "drag_action", "x" );
            if ( drag_action == 1 )
                suggested_action = GDK_ACTION_COPY;
            else if ( drag_action == 2 )
                suggested_action = GDK_ACTION_MOVE;
            else if ( drag_action == 3 )
                suggested_action = GDK_ACTION_LINK;
            else
            {
                // automatic
                file_browser->pending_drag_status = 1;
                gtk_drag_get_data (widget, drag_context, target, time);
                suggested_action = gdk_drag_context_get_selected_action( drag_context );
            }
        }
        gdk_drag_status( drag_context, suggested_action, time );
    }
    return TRUE;
}

gboolean on_folder_view_drag_leave ( GtkWidget *widget, GdkDragContext *drag_context, guint time, PtkFileBrowser* file_browser ) {
    /*  Don't call the default handler  */
    g_signal_stop_emission_by_name( widget, "drag-leave" );
    file_browser->drag_source_dev = 0;
    file_browser->drag_source_inode = 0;

    if ( folder_view_auto_scroll_timer )
    {
        g_source_remove( folder_view_auto_scroll_timer );
        folder_view_auto_scroll_timer = 0;
    }
    return TRUE;
}


gboolean on_folder_view_drag_drop ( GtkWidget *widget, GdkDragContext *drag_context, gint x, gint y, guint time, PtkFileBrowser* file_browser ) {
    GdkAtom target = gdk_atom_intern( "text/uri-list", FALSE );
    /*  Don't call the default handler  */
    g_signal_stop_emission_by_name( widget, "drag-drop" );

    gtk_drag_get_data ( widget, drag_context, target, time );
    return TRUE;
}


void on_folder_view_drag_end ( GtkWidget *widget, GdkDragContext *drag_context, PtkFileBrowser* file_browser ) {
    if ( folder_view_auto_scroll_timer )
    {
        g_source_remove( folder_view_auto_scroll_timer );
        folder_view_auto_scroll_timer = 0;
    }
    if ( file_browser->view_mode == PTK_FB_COMPACT_VIEW )
    {
        exo_icon_view_set_drag_dest_item( EXO_ICON_VIEW( widget ), NULL, 0 );
    }
    else if ( file_browser->view_mode == PTK_FB_LIST_VIEW )
    {
        gtk_tree_view_set_drag_dest_row( GTK_TREE_VIEW( widget ), NULL, 0 );
    }
    file_browser->is_drag = FALSE;
}

void ptk_file_browser_rename_selected_files( PtkFileBrowser* file_browser, GList* files, char* cwd ) {
    GtkWidget * parent;
    VFSFileInfo* file;
    GList* l;

    if ( !file_browser )
        return;
    gtk_widget_grab_focus( file_browser->folder_view );
    parent = gtk_widget_get_toplevel( GTK_WIDGET( file_browser ) );

    if ( ! files )
        return;

    for ( l = files; l; l = l->next )
    {
        file = (VFSFileInfo*)l->data;
        if ( !ptk_rename_file( NULL, file_browser, cwd, file, NULL, FALSE, PTK_RENAME, NULL ) )
            break;
    }
}

gboolean ptk_file_browser_can_paste( PtkFileBrowser* file_browser ) {
    /* FIXME: return FALSE when we don't have write permission */
    return FALSE;
}

void ptk_file_browser_paste( PtkFileBrowser* file_browser ) {
    GList * sel_files;
    VFSFileInfo* file;
    gchar* dest_dir = NULL;

    sel_files = ptk_file_browser_get_selected_files( file_browser );
//MOD removed - if you want this then at least make sure src != dest
/*    if ( sel_files && sel_files->next == NULL &&
            ( file = ( VFSFileInfo* ) sel_files->data ) &&
            vfs_file_info_is_dir( ( VFSFileInfo* ) sel_files->data ) )
    {
        dest_dir = g_build_filename( ptk_file_browser_get_cwd( file_browser ), vfs_file_info_get_name( file ), NULL );
    }
*/
    ptk_clipboard_paste_files( GTK_WINDOW( gtk_widget_get_toplevel( GTK_WIDGET( file_browser ) ) ),
                dest_dir ? dest_dir : ptk_file_browser_get_cwd( file_browser ), GTK_TREE_VIEW( file_browser->task_view ), NULL, NULL );
    if ( dest_dir )
        g_free( dest_dir );
    if ( sel_files )
    {
        g_list_foreach( sel_files, ( GFunc ) vfs_file_info_unref, NULL );
        g_list_free( sel_files );
    }
}

void ptk_file_browser_paste_link( PtkFileBrowser* file_browser ) {
    ptk_clipboard_paste_links( GTK_WINDOW( gtk_widget_get_toplevel( GTK_WIDGET( file_browser ) ) ),
                ptk_file_browser_get_cwd( file_browser ), GTK_TREE_VIEW( file_browser->task_view ), NULL, NULL );
}

void ptk_file_browser_paste_target( PtkFileBrowser* file_browser ) {
    ptk_clipboard_paste_targets( GTK_WINDOW( gtk_widget_get_toplevel( GTK_WIDGET( file_browser ) ) ),
                ptk_file_browser_get_cwd( file_browser ), GTK_TREE_VIEW( file_browser->task_view ), NULL, NULL );
}

gboolean ptk_file_browser_can_cut_or_copy( PtkFileBrowser* file_browser ) {
    return FALSE;
}

void ptk_file_browser_cut( PtkFileBrowser* file_browser ) {
    /* What "cut" and "copy" do are the same.
    *  The only difference is clipboard_action = GDK_ACTION_MOVE.
    */
    ptk_file_browser_cut_or_copy( file_browser, FALSE );
}

void ptk_file_browser_cut_or_copy( PtkFileBrowser* file_browser, gboolean copy ) {
    GList * sels;

    sels = ptk_file_browser_get_selected_files( file_browser );
    if ( ! sels )
        return ;
    ptk_clipboard_cut_or_copy_files( ptk_file_browser_get_cwd( file_browser ), sels, copy );
    vfs_file_info_list_free( sels );
}

void ptk_file_browser_copy( PtkFileBrowser* file_browser ) {
    ptk_file_browser_cut_or_copy( file_browser, TRUE );
}

gboolean ptk_file_browser_can_delete( PtkFileBrowser* file_browser ) {
    /* FIXME: return FALSE when we don't have write permission. */
    return TRUE;
}

void ptk_file_browser_delete( PtkFileBrowser* file_browser ) {
    GList * sel_files;
    GtkWidget* parent_win;

    if ( ! file_browser->n_sel_files )
        return ;
    sel_files = ptk_file_browser_get_selected_files( file_browser );
    parent_win = gtk_widget_get_toplevel( GTK_WIDGET( file_browser ) );
    ptk_delete_files( GTK_WINDOW( parent_win ), ptk_file_browser_get_cwd( file_browser ), sel_files, GTK_TREE_VIEW( file_browser->task_view ) );
    vfs_file_info_list_free( sel_files );
}

GList* ptk_file_browser_get_selected_files( PtkFileBrowser* file_browser ) {
    GList * sel_files;
    GList* sel;
    GList* file_list = NULL;
    GtkTreeModel* model;
    GtkTreeIter it;
    VFSFileInfo* file;

    sel_files = folder_view_get_selected_items( file_browser, &model );
    if ( !sel_files )
        return NULL;

    for ( sel = sel_files; sel; sel = g_list_next( sel ) )
    {
        gtk_tree_model_get_iter( model, &it, ( GtkTreePath* ) sel->data );
        gtk_tree_model_get( model, &it, COL_FILE_INFO, &file, -1 );
        file_list = g_list_append( file_list, file );
    }
    g_list_foreach( sel_files, ( GFunc ) gtk_tree_path_free, NULL );
    g_list_free( sel_files );
    return file_list;
}

void ptk_file_browser_open_selected_files_with_app( PtkFileBrowser* file_browser, char* app_desktop ) {
    GList * sel_files;
    sel_files = ptk_file_browser_get_selected_files( file_browser );

    ptk_open_files_with_app( ptk_file_browser_get_cwd( file_browser ), sel_files, app_desktop, NULL, file_browser, FALSE, FALSE );

    vfs_file_info_list_free( sel_files );
}

void ptk_file_browser_open_selected_files( PtkFileBrowser* file_browser ) {
    if ( xset_opener( NULL, file_browser, 1 ) )
        return;
    ptk_file_browser_open_selected_files_with_app( file_browser, NULL );
}

void ptk_file_browser_copycmd( PtkFileBrowser* file_browser, GList* sel_files, char* cwd, char* setname ) {
    if ( !setname || !file_browser || !sel_files )
        return;
    XSet* set2;
    char* copy_dest = NULL;
    char* move_dest = NULL;
    char* path;

    if ( !strcmp( setname, "copy_tab_prev" ) )
        copy_dest = main_window_get_tab_cwd( file_browser, -1 );
    else if ( !strcmp( setname, "copy_tab_next" ) )
        copy_dest = main_window_get_tab_cwd( file_browser, -2 );
    else if ( !strncmp( setname, "copy_tab_", 9 ) )
        copy_dest = main_window_get_tab_cwd( file_browser, atoi( setname + 9 ) );
    else if ( !strcmp( setname, "copy_panel_prev" ) )
        copy_dest = main_window_get_panel_cwd( file_browser, -1 );
    else if ( !strcmp( setname, "copy_panel_next" ) )
        copy_dest = main_window_get_panel_cwd( file_browser, -2 );
    else if ( !strncmp( setname, "copy_panel_", 11 ) )
        copy_dest = main_window_get_panel_cwd( file_browser, atoi( setname + 11 ) );
    else if ( !strcmp( setname, "copy_loc_last" ) )
    {
        set2 = xset_get( "copy_loc_last" );
        copy_dest = g_strdup( set2->desc );
    }
    else if ( !strcmp( setname, "move_tab_prev" ) )
        move_dest = main_window_get_tab_cwd( file_browser, -1 );
    else if ( !strcmp( setname, "move_tab_next" ) )
        move_dest = main_window_get_tab_cwd( file_browser, -2 );
    else if ( !strncmp( setname, "move_tab_", 9 ) )
        move_dest = main_window_get_tab_cwd( file_browser, atoi( setname + 9 ) );
    else if ( !strcmp( setname, "move_panel_prev" ) )
        move_dest = main_window_get_panel_cwd( file_browser, -1 );
    else if ( !strcmp( setname, "move_panel_next" ) )
        move_dest = main_window_get_panel_cwd( file_browser, -2 );
    else if ( !strncmp( setname, "move_panel_", 11 ) )
        move_dest = main_window_get_panel_cwd( file_browser, atoi( setname + 11 ) );
    else if ( !strcmp( setname, "move_loc_last" ) )
    {
        set2 = xset_get( "copy_loc_last" );
        move_dest = g_strdup( set2->desc );
    }

    if ( ( g_str_has_prefix( setname, "copy_loc" ) ||
                                g_str_has_prefix( setname, "move_loc" ) ) &&
                                                !copy_dest && !move_dest )
    {
        char* folder;
        set2 = xset_get( "copy_loc_last" );
        if ( set2->desc )
            folder = set2->desc;
        else
            folder = cwd;
        path = xset_file_dialog( GTK_WIDGET( file_browser ), GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER, _("Choose Location"), folder, NULL );
        if ( path && g_file_test( path, G_FILE_TEST_IS_DIR ) )
        {
            if ( g_str_has_prefix( setname, "copy_loc" ) )
                copy_dest = path;
            else
                move_dest = path;
            set2 = xset_get( "copy_loc_last" );
            xset_set_set( set2, "desc", path );
        }
        else
            return;
    }

    if ( copy_dest || move_dest )
    {
        int file_action;
        char* dest_dir;

        if ( copy_dest )
        {
            file_action = VFS_FILE_TASK_COPY;
            dest_dir = copy_dest;
        } else {
            file_action = VFS_FILE_TASK_MOVE;
            dest_dir = move_dest;
        }

        if ( !strcmp( dest_dir, cwd ) )
        {
            xset_msg_dialog( GTK_WIDGET( file_browser ), GTK_MESSAGE_ERROR, _("Invalid Destination"), NULL, 0, _("Destination same as source"), NULL, NULL );
            g_free( dest_dir );
            return;
        }

        // rebuild sel_files with full paths
        GList* file_list = NULL;
        GList* sel;
        char* file_path;
        VFSFileInfo* file;
        for ( sel = sel_files; sel; sel = sel->next )
        {
            file = ( VFSFileInfo* ) sel->data;
            file_path = g_build_filename( cwd, vfs_file_info_get_name( file ), NULL );
            file_list = g_list_prepend( file_list, file_path );
        }

        // task
        PtkFileTask* task = ptk_file_task_new( file_action, file_list, dest_dir,
                       GTK_WINDOW( gtk_widget_get_toplevel( GTK_WIDGET( file_browser ) ) ), file_browser->task_view );
        ptk_file_task_run( task );
        g_free( dest_dir );
    } else {
        xset_msg_dialog( GTK_WIDGET( file_browser ), GTK_MESSAGE_ERROR, _("Invalid Destination"), NULL, 0, _("Invalid destination"), NULL, NULL );
    }
}

void ptk_file_browser_hide_selected( PtkFileBrowser* file_browser, GList* files, char* cwd ) {
    if ( xset_msg_dialog( GTK_WIDGET( file_browser ), 0, _("Hide File"), NULL, GTK_BUTTONS_OK_CANCEL,
                _("The names of the selected files will be added to the '.hidden' file located in this folder, which will hide them from view in zzzFM.  You may need to refresh the view or restart zzzFM for the files to disappear.\n\nTo unhide a file, open the .hidden file in your text editor, remove the name of the file, and refresh."),
                NULL, NULL ) != GTK_RESPONSE_OK )
        return;

    VFSFileInfo* file;
    GList *l;

    if ( files )
    {
        for ( l = files; l; l = l->next )
        {
            file = ( VFSFileInfo* ) l->data;
            if ( !vfs_dir_add_hidden( cwd, vfs_file_info_get_name( file ) ) )
                ptk_show_error( GTK_WINDOW( gtk_widget_get_toplevel( GTK_WIDGET( file_browser ) ) ), _("Error"), _("Error hiding files") );
        }
        // refresh from here causes a segfault occasionally
        //ptk_file_browser_refresh( NULL, file_browser );
    }
    else
        ptk_show_error( GTK_WINDOW( gtk_widget_get_toplevel( GTK_WIDGET( file_browser ) ) ), _("Error"), _( "No files are selected" ) );
}

void ptk_file_browser_file_properties( PtkFileBrowser* file_browser, int page ) {
    GtkWidget * parent;
    GList* sel_files = NULL;
    char* dir_name = NULL;
    const char* cwd;

    if ( ! file_browser )
        return ;
    sel_files = ptk_file_browser_get_selected_files( file_browser );
    cwd = ptk_file_browser_get_cwd( file_browser );
    if ( !sel_files )
    {
        VFSFileInfo * file = vfs_file_info_new();
        vfs_file_info_get( file, ptk_file_browser_get_cwd( file_browser ), NULL );
        sel_files = g_list_prepend( NULL, file );
        dir_name = g_path_get_dirname( cwd );
    }
    parent = gtk_widget_get_toplevel( GTK_WIDGET( file_browser ) );
    ptk_show_file_properties( GTK_WINDOW( parent ), dir_name ? dir_name : cwd, sel_files, page );
    vfs_file_info_list_free( sel_files );
    g_free( dir_name );
}

void   on_popup_file_properties_activate ( GtkMenuItem *menuitem, gpointer user_data ) {
    GObject * popup = G_OBJECT( user_data );
    PtkFileBrowser* file_browser = ( PtkFileBrowser* ) g_object_get_data( popup, "PtkFileBrowser" );
    ptk_file_browser_file_properties( file_browser, 0 );
}

void ptk_file_browser_show_hidden_files( PtkFileBrowser* file_browser, gboolean show ) {
    if ( !!file_browser->show_hidden_files == show )
        return;
    file_browser->show_hidden_files = show;

    if ( file_browser->file_list )
    {
        ptk_file_browser_update_model( file_browser );
        g_signal_emit( file_browser, signals[ SEL_CHANGE_SIGNAL ], 0 );
    }

    if ( file_browser->side_dir )
    {
        ptk_dir_tree_view_show_hidden_files( GTK_TREE_VIEW( file_browser->side_dir ), file_browser->show_hidden_files );
    }

    ptk_file_browser_update_toolbar_widgets( file_browser, NULL, XSET_TOOL_SHOW_HIDDEN );
}

static gboolean on_dir_tree_button_press( GtkWidget* view, GdkEventButton* evt, PtkFileBrowser* file_browser ) {
    ptk_file_browser_focus_me( file_browser );

    if ( ( evt_win_click->s || evt_win_click->ob2_data ) &&
            main_window_event( file_browser->main_window, evt_win_click, "evt_win_click", 0, 0, "dirtree", 0, evt->button, evt->state, TRUE ) )
        return FALSE;

    if ( evt->type == GDK_BUTTON_PRESS && evt->button == 2 )    /* middle click */
    {
        /* left and right click handled in ptk-dir-tree-view.c
         * on_dir_tree_view_button_press() */
        GtkTreeModel * model;
        GtkTreePath* tree_path;
        GtkTreeIter it;

        model = gtk_tree_view_get_model( GTK_TREE_VIEW( view ) );
        if ( gtk_tree_view_get_path_at_pos( GTK_TREE_VIEW( view ), evt->x, evt->y, &tree_path, NULL, NULL, NULL ) )
        {
            if ( gtk_tree_model_get_iter( model, &it, tree_path ) )
            {
                VFSFileInfo * file;
                gtk_tree_model_get( model, &it, COL_DIR_TREE_INFO, &file, -1 );
                if ( file )
                {
                    char* file_path;
                    file_path = ptk_dir_view_get_dir_path( model, &it );
                    g_signal_emit( file_browser, signals[ OPEN_ITEM_SIGNAL ], 0, file_path, PTK_OPEN_NEW_TAB );
                    g_free( file_path );
                    vfs_file_info_unref( file );
                }
            }
            gtk_tree_path_free( tree_path );
        }
        return TRUE;
    }
    return FALSE;
}


GtkWidget* ptk_file_browser_create_dir_tree( PtkFileBrowser* file_browser ) {
    GtkWidget * dir_tree;
    GtkTreeSelection* dir_tree_sel;
    dir_tree = ptk_dir_tree_view_new( file_browser, file_browser->show_hidden_files );
    dir_tree_sel = gtk_tree_view_get_selection( GTK_TREE_VIEW( dir_tree ) );
    g_signal_connect ( dir_tree, "row-activated", G_CALLBACK ( on_dir_tree_row_activated ), file_browser );
    g_signal_connect ( dir_tree, "button-press-event", G_CALLBACK ( on_dir_tree_button_press ), file_browser );

    // set font
    if ( xset_get_s_panel( file_browser->mypanel, "font_file" ) )
    {
        PangoFontDescription* font_desc = pango_font_description_from_string( xset_get_s_panel( file_browser->mypanel, "font_file" ) );
        gtk_widget_modify_font( dir_tree, font_desc );
        pango_font_description_free( font_desc );
    }

    return dir_tree;
}

int file_list_order_from_sort_order( PtkFBSortOrder order ) {
    int col;

    switch ( order )
    {
    case PTK_FB_SORT_BY_NAME:
        col = COL_FILE_NAME;
        break;
    case PTK_FB_SORT_BY_SIZE:
        col = COL_FILE_SIZE;
        break;
    case PTK_FB_SORT_BY_MTIME:
        col = COL_FILE_MTIME;
        break;
    case PTK_FB_SORT_BY_TYPE:
        col = COL_FILE_DESC;
        break;
    case PTK_FB_SORT_BY_PERM:
        col = COL_FILE_PERM;
        break;
    case PTK_FB_SORT_BY_OWNER:
        col = COL_FILE_OWNER;
        break;
    default:
        col = COL_FILE_NAME;
    }
    return col;
}

void ptk_file_browser_read_sort_extra( PtkFileBrowser* file_browser ) {
    PtkFileList* list = PTK_FILE_LIST( file_browser->file_list );
    if ( !list )
        return;

    list->sort_natural = xset_get_b_panel( file_browser->mypanel, "sort_extra" );
    list->sort_case = xset_get_int_panel( file_browser->mypanel, "sort_extra", "x" ) == XSET_B_TRUE;
    list->sort_dir = xset_get_int_panel( file_browser->mypanel, "sort_extra", "y" );
    list->sort_hidden_first = xset_get_int_panel( file_browser->mypanel, "sort_extra", "z" ) == XSET_B_TRUE;
}

void ptk_file_browser_set_sort_extra( PtkFileBrowser* file_browser, const char* setname ) {
    if ( !file_browser || !setname )
        return;

    XSet* set = xset_get( setname );

    if ( !g_str_has_prefix( set->name, "sortx_" ) )
        return;

    const char* name = set->name + 6;
    PtkFileList* list = PTK_FILE_LIST( file_browser->file_list );
    if ( !list )
        return;
    int panel = file_browser->mypanel;
    char* val = NULL;

    if ( !strcmp( name, "natural" ) )
    {
        list->sort_natural = set->b == XSET_B_TRUE;
        xset_set_b_panel( panel, "sort_extra", list->sort_natural );
    }
    else if ( !strcmp( name, "case" ) )
    {
        list->sort_case = set->b == XSET_B_TRUE;
        val = g_strdup_printf( "%d", set->b );
        xset_set_panel( panel, "sort_extra", "x", val );
    }
    else if ( !strcmp( name, "folders" ) )
    {
        list->sort_dir = PTK_LIST_SORT_DIR_FIRST;
        val = g_strdup_printf( "%d", PTK_LIST_SORT_DIR_FIRST );
        xset_set_panel( panel, "sort_extra", "y", val );
    }
    else if ( !strcmp( name, "files" ) )
    {
        list->sort_dir = PTK_LIST_SORT_DIR_LAST;
        val = g_strdup_printf( "%d", PTK_LIST_SORT_DIR_LAST );
        xset_set_panel( panel, "sort_extra", "y", val );
    }
    else if ( !strcmp( name, "mix" ) )
    {
        list->sort_dir = PTK_LIST_SORT_DIR_MIXED;
        val = g_strdup_printf( "%d", PTK_LIST_SORT_DIR_MIXED );
        xset_set_panel( panel, "sort_extra", "y", val );
    }
    else if ( !strcmp( name, "hidfirst" ) )
    {
        list->sort_hidden_first = set->b == XSET_B_TRUE;
        val = g_strdup_printf( "%d", set->b );
        xset_set_panel( panel, "sort_extra", "z", val );
    }
    else if ( !strcmp( name, "hidlast" ) )
    {
        list->sort_hidden_first = set->b != XSET_B_TRUE;
        val = g_strdup_printf( "%d", set->b == XSET_B_TRUE ?  XSET_B_FALSE : XSET_B_TRUE );
        xset_set_panel( panel, "sort_extra", "z", val );
    }
    g_free( val );
    ptk_file_list_sort( list );
}

void ptk_file_browser_set_sort_order( PtkFileBrowser* file_browser, PtkFBSortOrder order ) {
    int col;
    if ( order == file_browser->sort_order )
        return ;

    file_browser->sort_order = order;
    col = file_list_order_from_sort_order( order );

    if ( file_browser->file_list )
    {
        gtk_tree_sortable_set_sort_column_id( GTK_TREE_SORTABLE( file_browser->file_list ), col, file_browser->sort_type );
    }
}

void ptk_file_browser_set_sort_type( PtkFileBrowser* file_browser, GtkSortType order ) {
    int col;
    GtkSortType old_order;

    if ( order != file_browser->sort_type )
    {
        file_browser->sort_type = order;
        if ( file_browser->file_list )
        {
            gtk_tree_sortable_get_sort_column_id( GTK_TREE_SORTABLE( file_browser->file_list ), &col, &old_order );
            gtk_tree_sortable_set_sort_column_id( GTK_TREE_SORTABLE( file_browser->file_list ), col, order );
        }
    }
}

PtkFBSortOrder ptk_file_browser_get_sort_order( PtkFileBrowser* file_browser ) {
    return file_browser->sort_order;
}

GtkSortType ptk_file_browser_get_sort_type( PtkFileBrowser* file_browser ) {
    return file_browser->sort_type;
}



/* FIXME: Don't recreate the view if previous view is icon view */
void ptk_file_browser_view_as_compact_list( PtkFileBrowser* file_browser ) {
    if ( file_browser->view_mode == PTK_FB_COMPACT_VIEW &&  file_browser->folder_view )
        return ;

    show_thumbnails( file_browser, PTK_FILE_LIST( file_browser->file_list ), file_browser->large_icons, file_browser->max_thumbnail );

    file_browser->view_mode = PTK_FB_COMPACT_VIEW;
    if ( file_browser->folder_view )
        gtk_widget_destroy( file_browser->folder_view );
    file_browser->folder_view = create_folder_view( file_browser, PTK_FB_COMPACT_VIEW );
    exo_icon_view_set_model( EXO_ICON_VIEW( file_browser->folder_view ), file_browser->file_list );
    gtk_scrolled_window_set_policy( GTK_SCROLLED_WINDOW( file_browser->folder_view_scroll ), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC );
    gtk_widget_show( file_browser->folder_view );
    gtk_container_add( GTK_CONTAINER( file_browser->folder_view_scroll ), file_browser->folder_view );
}

void ptk_file_browser_view_as_list ( PtkFileBrowser* file_browser ) {
    if ( file_browser->view_mode == PTK_FB_LIST_VIEW && file_browser->folder_view )
        return ;

    show_thumbnails( file_browser, PTK_FILE_LIST( file_browser->file_list ), file_browser->large_icons, file_browser->max_thumbnail );

    file_browser->view_mode = PTK_FB_LIST_VIEW;
    if ( file_browser->folder_view )
        gtk_widget_destroy( file_browser->folder_view );
    file_browser->folder_view = create_folder_view( file_browser, PTK_FB_LIST_VIEW );
    gtk_tree_view_set_model( GTK_TREE_VIEW( file_browser->folder_view ), file_browser->file_list );
    gtk_scrolled_window_set_policy( GTK_SCROLLED_WINDOW( file_browser->folder_view_scroll ), GTK_POLICY_AUTOMATIC, GTK_POLICY_ALWAYS );
    gtk_widget_show( file_browser->folder_view );
    gtk_container_add( GTK_CONTAINER( file_browser->folder_view_scroll ), file_browser->folder_view );

}

void ptk_file_browser_create_new_file( PtkFileBrowser* file_browser, gboolean create_folder ) {
    VFSFileInfo *file = NULL;
    if( ptk_create_new_file( GTK_WINDOW( gtk_widget_get_toplevel( GTK_WIDGET( file_browser ) ) ), ptk_file_browser_get_cwd( file_browser ), create_folder, &file ) )
    {
        PtkFileList* list = PTK_FILE_LIST( file_browser->file_list );
        GtkTreeIter it;
        /* generate created event before FAM to enhance responsiveness. */
        vfs_dir_emit_file_created( file_browser->dir, vfs_file_info_get_name(file), TRUE );

        /* select the created file */
        if( ptk_file_list_find_iter( list, &it, file ) )
        {
            GtkTreePath* tp = gtk_tree_model_get_path( GTK_TREE_MODEL(list), &it );
            if ( file_browser->view_mode == PTK_FB_COMPACT_VIEW )
            {
                exo_icon_view_select_path( EXO_ICON_VIEW( file_browser->folder_view ), tp );
                exo_icon_view_set_cursor( EXO_ICON_VIEW( file_browser->folder_view ), tp, NULL, FALSE );

                /* NOTE for dirty hack:
                 *  Layout of icon view is done in idle handler,
                 *  so we have to let it re-layout after the insertion of new item.
                  * or we cannot scroll to the specified path correctly.  */
                while( gtk_events_pending() )
                    gtk_main_iteration();
                exo_icon_view_scroll_to_path( EXO_ICON_VIEW( file_browser->folder_view ), tp, FALSE, 0, 0 );
            }
            else if ( file_browser->view_mode == PTK_FB_LIST_VIEW )
            {
                //MOD  give new folder/file focus

                //GtkTreeSelection * tree_sel;
                //tree_sel = gtk_tree_view_get_selection( GTK_TREE_VIEW( file_browser->folder_view ) );
                //gtk_tree_selection_select_iter( tree_sel, &it );

                GtkTreeSelection *selection;
                GList            *selected_paths = NULL;
                GList            *lp;
                /* save selected paths */
                selection = gtk_tree_view_get_selection( GTK_TREE_VIEW( file_browser->folder_view ) );
                selected_paths = gtk_tree_selection_get_selected_rows( selection, NULL );

                gtk_tree_view_set_cursor( GTK_TREE_VIEW( file_browser->folder_view ), tp, NULL, FALSE);
                /* select all previously selected paths */
                for (lp = selected_paths; lp != NULL; lp = lp->next)
                gtk_tree_selection_select_path (selection, lp->data);

                /*
                while( gtk_events_pending() )
                    gtk_main_iteration();

                */
                gtk_tree_view_scroll_to_cell( GTK_TREE_VIEW( file_browser->folder_view ), tp, NULL, FALSE, 0, 0 );
            }
            gtk_widget_grab_focus( GTK_WIDGET( file_browser->folder_view ) );
            gtk_tree_path_free( tp );
        }
        vfs_file_info_unref( file );
    }
}

guint ptk_file_browser_get_n_sel( PtkFileBrowser* file_browser, guint64* sel_size ) {
    if ( sel_size )
        *sel_size = file_browser->sel_size;
    return file_browser->n_sel_files;
}

void ptk_file_browser_before_chdir( PtkFileBrowser* file_browser, const char* path, gboolean* cancel ) {}

void ptk_file_browser_after_chdir( PtkFileBrowser* file_browser ) {}

void ptk_file_browser_content_change( PtkFileBrowser* file_browser ) {}

void ptk_file_browser_sel_change( PtkFileBrowser* file_browser ) {}

void ptk_file_browser_pane_mode_change( PtkFileBrowser* file_browser ) {}

void ptk_file_browser_open_item( PtkFileBrowser* file_browser, const char* path, int action ) {}

void ptk_file_browser_show_shadow( PtkFileBrowser* file_browser ) {
    gtk_scrolled_window_set_shadow_type ( GTK_SCROLLED_WINDOW ( file_browser->folder_view_scroll ), GTK_SHADOW_IN );
}

void ptk_file_browser_hide_shadow( PtkFileBrowser* file_browser ) {
    gtk_scrolled_window_set_shadow_type ( GTK_SCROLLED_WINDOW ( file_browser->folder_view_scroll ), GTK_SHADOW_NONE );
}

void show_thumbnails( PtkFileBrowser* file_browser, PtkFileList* list, gboolean is_big, int max_file_size ) {
    /* This function collects all calls to ptk_file_list_show_thumbnails()
     * and disables them if change detection is blacklisted on current device */
    if ( !( file_browser && file_browser->dir ) )
        max_file_size = 0;
    else if ( file_browser->dir->avoid_changes )
        max_file_size = 0;
    ptk_file_list_show_thumbnails( list, is_big, max_file_size );
    ptk_file_browser_update_toolbar_widgets( file_browser, NULL, XSET_TOOL_SHOW_THUMB );
}

void ptk_file_browser_show_thumbnails( PtkFileBrowser* file_browser, int max_file_size ) {
    file_browser->max_thumbnail = max_file_size;
    if ( file_browser->file_list )
    {
        show_thumbnails( file_browser, PTK_FILE_LIST( file_browser->file_list ), file_browser->large_icons, max_file_size );
    }
}

#if 0
// no longer used because icon size changes required a rebuild of folder_view
// due to text renderer not resizing
void ptk_file_browser_update_display( PtkFileBrowser* file_browser ) {
    GtkTreeSelection * tree_sel;
    GList *sel = NULL, *l;
    GtkTreePath* tree_path;
    int big_icon_size, small_icon_size;

    if ( ! file_browser->file_list )
        return ;
    g_object_ref( G_OBJECT( file_browser->file_list ) );

    if ( file_browser->max_thumbnail )
        show_thumbnails( file_browser, PTK_FILE_LIST( file_browser->file_list ), file_browser->large_icons, file_browser->max_thumbnail );

    vfs_mime_type_get_icon_size( &big_icon_size, &small_icon_size );

    if ( file_browser->view_mode == PTK_FB_COMPACT_VIEW )
    {
        sel = exo_icon_view_get_selected_items( EXO_ICON_VIEW( file_browser->folder_view ) );

        exo_icon_view_set_model( EXO_ICON_VIEW( file_browser->folder_view ), NULL );
        if( file_browser->view_mode == PTK_FB_COMPACT_VIEW )
            gtk_cell_renderer_set_fixed_size( file_browser->icon_render, file_browser->large_icons ? big_icon_size : small_icon_size,
                                                                         file_browser->large_icons ? big_icon_size :  small_icon_size );
        exo_icon_view_set_model( EXO_ICON_VIEW( file_browser->folder_view ), GTK_TREE_MODEL( file_browser->file_list ) );

        for ( l = sel; l; l = l->next )
        {
            tree_path = ( GtkTreePath* ) l->data;
            exo_icon_view_select_path( EXO_ICON_VIEW( file_browser->folder_view ), tree_path );
            gtk_tree_path_free( tree_path );
        }
    }
    else if ( file_browser->view_mode == PTK_FB_LIST_VIEW )
    {
        tree_sel = gtk_tree_view_get_selection( GTK_TREE_VIEW( file_browser->folder_view ) );
        sel = gtk_tree_selection_get_selected_rows( tree_sel, NULL );

        gtk_tree_view_set_model( GTK_TREE_VIEW( file_browser->folder_view ), NULL );
        gtk_cell_renderer_set_fixed_size( file_browser->icon_render, file_browser->large_icons ? big_icon_size : small_icon_size,
                                                                     file_browser->large_icons ? big_icon_size : small_icon_size );
        gtk_tree_view_set_model( GTK_TREE_VIEW( file_browser->folder_view ), GTK_TREE_MODEL( file_browser->file_list ) );

        for ( l = sel; l; l = l->next )
        {
            tree_path = ( GtkTreePath* ) l->data;
            gtk_tree_selection_select_path( tree_sel, tree_path );
            gtk_tree_path_free( tree_path );
        }
    }
    g_list_free( sel );
    g_object_unref( G_OBJECT( file_browser->file_list ) );
}
#endif

void ptk_file_browser_emit_open( PtkFileBrowser* file_browser, const char* path, PtkOpenAction action ) {
    g_signal_emit( file_browser, signals[ OPEN_ITEM_SIGNAL ], 0, path, action );
}

void ptk_file_browser_set_single_click( PtkFileBrowser* file_browser, gboolean single_click ) {
    if( single_click == file_browser->single_click )
        return;
    if( file_browser->view_mode == PTK_FB_LIST_VIEW )
        exo_tree_view_set_single_click( (ExoTreeView*)file_browser->folder_view, single_click );
    else if( file_browser->view_mode == PTK_FB_COMPACT_VIEW )
        exo_icon_view_set_single_click( (ExoIconView*)file_browser->folder_view, single_click );
    file_browser->single_click = single_click;
}

void ptk_file_browser_set_single_click_timeout( PtkFileBrowser* file_browser, guint timeout ) {
    if( timeout == file_browser->single_click_timeout )
        return;
    if( file_browser->view_mode == PTK_FB_LIST_VIEW )
        exo_tree_view_set_single_click_timeout( (ExoTreeView*)file_browser->folder_view, timeout );
    else if( file_browser->view_mode == PTK_FB_COMPACT_VIEW )
        exo_icon_view_set_single_click_timeout( (ExoIconView*)file_browser->folder_view, timeout );
    file_browser->single_click_timeout = timeout;
}

////////////////////////////////////////////////////////////////////////////

int ptk_file_browser_no_access( const char* cwd, const char* smode ) {
    int mode;
    if ( !smode )
        mode = W_OK;
    else if ( !strcmp( smode, "R_OK" ) )
        mode = R_OK;
    else
        mode = W_OK;

    int no_access = 0;
    #if defined(HAVE_EUIDACCESS)
        no_access = euidaccess( cwd, mode );
    #elif defined(HAVE_EACCESS)
        no_access = eaccess( cwd, mode );
    #endif
    return no_access;
}

void ptk_file_browser_find_file( GtkMenuItem *menuitem, PtkFileBrowser* file_browser ) {
    const char* cwd;
    const char* dirs[2];
    cwd = ptk_file_browser_get_cwd( file_browser );

    dirs[0] = cwd;
    dirs[1] = NULL;
    fm_find_files( dirs );
}

void ptk_file_browser_focus( GtkMenuItem *item, PtkFileBrowser* file_browser, int job2 ) {
    GtkWidget* widget;
    int job;
    if ( item )
        job = GPOINTER_TO_INT( g_object_get_data( G_OBJECT( item ), "job" ) );
    else
        job = job2;

    FMMainWindow* main_window = (FMMainWindow*)file_browser->main_window;
    int p = file_browser->mypanel;
    char mode = main_window->panel_context[p-1];
    switch ( job )
    {
        case 0:
            // path bar
            if ( !xset_get_b_panel_mode( p, "show_toolbox", mode ) )
            {
                xset_set_b_panel_mode( p, "show_toolbox", mode, TRUE );
                update_views_all_windows( NULL, file_browser );
            }
            widget = file_browser->path_bar;
            break;
        case 1:
            if ( !xset_get_b_panel_mode( p, "show_dirtree", mode ) )
            {
                xset_set_b_panel_mode( p, "show_dirtree", mode, TRUE );
                update_views_all_windows( NULL, file_browser );
            }
            widget = file_browser->side_dir;
            break;
        case 2:
            if ( !xset_get_b_panel_mode( p, "show_book", mode ) )
            {
                xset_set_b_panel_mode( p, "show_book", mode, TRUE );
                update_views_all_windows( NULL, file_browser );
            }
            widget = file_browser->side_book;
            break;
        case 3:
            if ( !xset_get_b_panel_mode( p, "show_devmon", mode ) )
            {
                xset_set_b_panel_mode( p, "show_devmon", mode, TRUE );
                update_views_all_windows( NULL, file_browser );
            }
            widget = file_browser->side_dev;
            break;
        case 4:
            widget = file_browser->folder_view;
            break;
        default:
            return;
    }
    if ( gtk_widget_get_visible( widget ) )
        gtk_widget_grab_focus( GTK_WIDGET( widget ) );
}

void focus_folder_view( PtkFileBrowser* file_browser ) {
    gtk_widget_grab_focus( GTK_WIDGET( file_browser->folder_view ) );
    g_signal_emit( file_browser, signals[ PANE_MODE_CHANGE_SIGNAL ], 0 );
}

void ptk_file_browser_focus_me( PtkFileBrowser* file_browser ) {
    g_signal_emit( file_browser, signals[ PANE_MODE_CHANGE_SIGNAL ], 0 );
}

void ptk_file_browser_go_tab( GtkMenuItem *item, PtkFileBrowser* file_browser, int t ) {
//printf( "ptk_file_browser_go_tab fb=%#x\n", file_browser );
    GtkWidget* notebook = file_browser->mynotebook;
    int tab_num;
    if ( item )
        tab_num = GPOINTER_TO_INT( g_object_get_data( G_OBJECT( item ), "tab_num" ) );
    else
        tab_num = t;

    if ( tab_num == -1 )  // prev
    {
        if ( gtk_notebook_get_current_page( GTK_NOTEBOOK( notebook ) ) == 0 )
            gtk_notebook_set_current_page( GTK_NOTEBOOK( notebook ), gtk_notebook_get_n_pages( GTK_NOTEBOOK( notebook ) ) - 1 );
        else
            gtk_notebook_prev_page( GTK_NOTEBOOK( notebook ) );
    }
    else if ( tab_num == -2 )  // next
    {
        if ( gtk_notebook_get_current_page( GTK_NOTEBOOK( notebook ) ) + 1 == gtk_notebook_get_n_pages( GTK_NOTEBOOK( notebook ) ) )
            gtk_notebook_set_current_page( GTK_NOTEBOOK( notebook ), 0 );
        else
            gtk_notebook_next_page( GTK_NOTEBOOK( notebook ) );
    }
    else if ( tab_num == -3 )  // close
    {
        on_close_notebook_page( NULL, file_browser );
    } else {
        if ( tab_num <= gtk_notebook_get_n_pages( GTK_NOTEBOOK( notebook ) ) &&  tab_num > 0 )
            gtk_notebook_set_current_page( GTK_NOTEBOOK( notebook ), tab_num - 1 );
    }
}

void ptk_file_browser_open_in_tab( PtkFileBrowser* file_browser, int tab_num, char* file_path ) {
    int page_x;
    GtkWidget* notebook = file_browser->mynotebook;
    int cur_page = gtk_notebook_get_current_page( GTK_NOTEBOOK( notebook ) );
    int pages = gtk_notebook_get_n_pages( GTK_NOTEBOOK( notebook ) );

    if ( tab_num == -1 )  // prev
        page_x = cur_page - 1;
    else if ( tab_num == -2 )  // next
        page_x = cur_page + 1;
    else
        page_x = tab_num - 1;

    if ( page_x > -1 && page_x < pages && page_x != cur_page )
    {
        PtkFileBrowser* a_browser = (PtkFileBrowser*)gtk_notebook_get_nth_page( GTK_NOTEBOOK( notebook ), page_x );

        ptk_file_browser_chdir( a_browser, file_path, PTK_FB_CHDIR_ADD_HISTORY );
    }
}

void ptk_file_browser_on_permission( GtkMenuItem* item, PtkFileBrowser* file_browser, GList* sel_files, char* cwd ) {
    char* name;
    char* cmd;
    const char* prog;
    gboolean as_root = FALSE;
    char* user1 = "1000";
    char* user2 = "1001";
    char* myuser = g_strdup_printf( "%d", geteuid() );

    if ( !sel_files )
        return;

    XSet* set = (XSet*)g_object_get_data( G_OBJECT( item ), "set" );
    if ( !set || !file_browser )
        return;

    if ( !strncmp( set->name, "perm_", 5 ) )
    {
        name = set->name + 5;
        if ( !strncmp( name, "go", 2 ) || !strncmp( name, "ugo", 3 ) )
            prog = "chmod -R";
        else
            prog = "chmod";
    }
    else if ( !strncmp( set->name, "rperm_", 6 ) )
    {
        name = set->name + 6;
        if ( !strncmp( name, "go", 2 ) || !strncmp( name, "ugo", 3 ) )
            prog = "chmod -R";
        else
            prog = "chmod";
        as_root = TRUE;
    }
    else if ( !strncmp( set->name, "own_", 4 ) )
    {
        name = set->name + 4;
        prog = "chown";
        as_root = TRUE;
    }
    else if ( !strncmp( set->name, "rown_", 5 ) )
    {
        name = set->name + 5;
        prog = "chown -R";
        as_root = TRUE;
    }
    else
        return;

    if ( !strcmp( name, "r" ) )
        cmd = g_strdup_printf( "u+r-wx,go-rwx" );
    else if ( !strcmp( name, "rw" ) )
        cmd = g_strdup_printf( "u+rw-x,go-rwx" );
    else if ( !strcmp( name, "rwx" ) )
        cmd = g_strdup_printf( "u+rwx,go-rwx" );
    else if ( !strcmp( name, "r_r" ) )
        cmd = g_strdup_printf( "u+r-wx,g+r-wx,o-rwx" );
    else if ( !strcmp( name, "rw_r" ) )
        cmd = g_strdup_printf( "u+rw-x,g+r-wx,o-rwx" );
    else if ( !strcmp( name, "rw_rw" ) )
        cmd = g_strdup_printf( "u+rw-x,g+rw-x,o-rwx" );
    else if ( !strcmp( name, "rwxr_x" ) )
        cmd = g_strdup_printf( "u+rwx,g+rx-w,o-rwx" );
    else if ( !strcmp( name, "rwxrwx" ) )
        cmd = g_strdup_printf( "u+rwx,g+rwx,o-rwx" );
    else if ( !strcmp( name, "r_r_r" ) )
        cmd = g_strdup_printf( "ugo+r,ugo-wx" );
    else if ( !strcmp( name, "rw_r_r" ) )
        cmd = g_strdup_printf( "u+rw-x,go+r-wx" );
    else if ( !strcmp( name, "rw_rw_rw" ) )
        cmd = g_strdup_printf( "ugo+rw-x" );
    else if ( !strcmp( name, "rwxr_r" ) )
        cmd = g_strdup_printf( "u+rwx,go+r-wx" );
    else if ( !strcmp( name, "rwxr_xr_x" ) )
        cmd = g_strdup_printf( "u+rwx,go+rx-w" );
    else if ( !strcmp( name, "rwxrwxrwx" ) )
        cmd = g_strdup_printf( "ugo+rwx,-t" );
    else if ( !strcmp( name, "rwxrwxrwt" ) )
        cmd = g_strdup_printf( "ugo+rwx,+t" );
    else if ( !strcmp( name, "unstick" ) )
        cmd = g_strdup_printf( "-t" );
    else if ( !strcmp( name, "stick" ) )
        cmd = g_strdup_printf( "+t" );
    else if ( !strcmp( name, "go_w" ) )
        cmd = g_strdup_printf( "go-w" );
    else if ( !strcmp( name, "go_rwx" ) )
        cmd = g_strdup_printf( "go-rwx" );
    else if ( !strcmp( name, "ugo_w" ) )
        cmd = g_strdup_printf( "ugo+w" );
    else if ( !strcmp( name, "ugo_rx" ) )
        cmd = g_strdup_printf( "ugo+rX" );
    else if ( !strcmp( name, "ugo_rwx" ) )
        cmd = g_strdup_printf( "ugo+rwX" );
    else if ( !strcmp( name, "myuser" ) )
        cmd = g_strdup_printf( "%s:%s", myuser, myuser );
    else if ( !strcmp( name, "myuser_users" ) )
        cmd = g_strdup_printf( "%s:users", myuser );
    else if ( !strcmp( name, "user1" ) )
        cmd = g_strdup_printf( "%s:%s", user1, user1 );
    else if ( !strcmp( name, "user1_users" ) )
        cmd = g_strdup_printf( "%s:users", user1 );
    else if ( !strcmp( name, "user2" ) )
        cmd = g_strdup_printf( "%s:%s", user2, user2 );
    else if ( !strcmp( name, "user2_users" ) )
        cmd = g_strdup_printf( "%s:users", user2 );
    else if ( !strcmp( name, "root" ) )
        cmd = g_strdup_printf( "root:root" );
    else if ( !strcmp( name, "root_users" ) )
        cmd = g_strdup_printf( "root:users" );
    else if ( !strcmp( name, "root_myuser" ) )
        cmd = g_strdup_printf( "root:%s", myuser );
    else if ( !strcmp( name, "root_user1" ) )
        cmd = g_strdup_printf( "root:%s", user1 );
    else if ( !strcmp( name, "root_user2" ) )
        cmd = g_strdup_printf( "root:%s", user2 );
    else
        return;

    char* file_paths = g_strdup( "" );
    GList* sel;
    char* file_path;
    char* str;
    for ( sel = sel_files; sel; sel = sel->next )
    {
        file_path = bash_quote( vfs_file_info_get_name( ( VFSFileInfo* ) sel->data ) );
        str = file_paths;
        file_paths = g_strdup_printf( "%s %s", file_paths, file_path );
        g_free( str );
        g_free( file_path );
    }

    // task
    PtkFileTask* task = ptk_file_exec_new( set->menu_label, cwd, GTK_WIDGET( file_browser ), file_browser->task_view );
    task->task->exec_command = g_strdup_printf( "%s %s %s", prog, cmd, file_paths );
    g_free( cmd );
    g_free( file_paths );
    task->task->exec_browser = file_browser;
    task->task->exec_sync = TRUE;
    task->task->exec_show_error = TRUE;
    task->task->exec_show_output = FALSE;
    task->task->exec_export = FALSE;
    if ( as_root )
        task->task->exec_as_user = g_strdup_printf( "root" );
    ptk_file_task_run( task );
}

void ptk_file_browser_on_action( PtkFileBrowser* browser, char* setname ) {
    char* xname;
    int i;
    XSet* set = xset_get( setname );
    XSet* set2;
    FMMainWindow* main_window = (FMMainWindow*)browser->main_window;
    char mode = main_window->panel_context[browser->mypanel-1];

//printf("ptk_file_browser_on_action %s\n", set->name );

    if ( g_str_has_prefix( set->name, "book_" ) )
    {
        xname = set->name + 5;
        if ( !strcmp( xname, "icon" ) || !strcmp( xname, "menu_icon" ) )
            ptk_bookmark_view_update_icons( NULL, browser );
        else if ( !strcmp( xname, "add" ) )
        {
            const char* text = browser->path_bar &&  gtk_widget_has_focus( browser->path_bar ) ?
                            gtk_entry_get_text( GTK_ENTRY( browser->path_bar ) ) :  NULL;
            if ( text && ( g_file_test( text, G_FILE_TEST_EXISTS ) ||  strstr( text, ":/" ) ||  g_str_has_prefix( text, "//" ) ) )
                ptk_bookmark_view_add_bookmark( NULL, browser, text );
            else
                ptk_bookmark_view_add_bookmark( NULL, browser, NULL );
        }
        else if ( !strcmp( xname, "open" ) && browser->side_book )
            ptk_bookmark_view_on_open_reverse( NULL, browser );
    }
    else if ( g_str_has_prefix( set->name, "go_" ) )
    {
        xname = set->name + 3;
        if ( !strcmp( xname, "back" ) )
            ptk_file_browser_go_back( NULL, browser );
        else if ( !strcmp( xname, "forward" ) )
            ptk_file_browser_go_forward( NULL, browser );
        else if ( !strcmp( xname, "up" ) )
            ptk_file_browser_go_up( NULL, browser );
        else if ( !strcmp( xname, "home" ) )
            ptk_file_browser_go_home( NULL, browser );
        else if ( !strcmp( xname, "default" ) )
            ptk_file_browser_go_default( NULL, browser );
        else if ( !strcmp( xname, "set_default" ) )
            ptk_file_browser_set_default_folder( NULL, browser );
    }
    else if ( g_str_has_prefix( set->name, "tab_" ) )
    {
        xname = set->name + 4;
        if ( !strcmp( xname, "new" ) )
            ptk_file_browser_new_tab( NULL, browser );
        else if ( !strcmp( xname, "new_here" ) )
            ptk_file_browser_new_tab_here( NULL, browser );
        else
        {
            if ( !strcmp( xname, "prev" ) )
                i = -1;
            else if ( !strcmp( xname, "next" ) )
                i = -2;
            else if ( !strcmp( xname, "close" ) )
                i = -3;
            else
                i = atoi( xname );
            ptk_file_browser_go_tab( NULL, browser, i );
        }
    }
    else if ( g_str_has_prefix( set->name, "focus_" ) )
    {
        xname = set->name + 6;
        if ( !strcmp( xname, "path_bar" ) )
            i = 0;
        else if ( !strcmp( xname, "filelist" ) )
            i = 4;
        else if ( !strcmp( xname, "dirtree" ) )
            i = 1;
        else if ( !strcmp( xname, "book" ) )
            i = 2;
        else if ( !strcmp( xname, "device" ) )
            i = 3;
        ptk_file_browser_focus( NULL, browser, i );
    }
    else if ( !strcmp( set->name, "view_reorder_col" ) )
        on_reorder( NULL, GTK_WIDGET( browser ) );
    else if ( !strcmp( set->name, "view_refresh" ) )
        ptk_file_browser_refresh( NULL, browser );
    else if ( !strcmp( set->name, "view_thumb" ) )
        main_window_toggle_thumbnails_all_windows();
    else if ( g_str_has_prefix( set->name, "sortby_" ) )
    {
        xname = set->name + 7;     // howdy     value of context offset WAS 7
        i = -3;
        if ( !strcmp( xname, "name" ) )
            i = PTK_FB_SORT_BY_NAME;
        else if ( !strcmp( xname, "size" ) )
            i = PTK_FB_SORT_BY_SIZE;
        else if ( !strcmp( xname, "type" ) )
            i = PTK_FB_SORT_BY_TYPE;
        else if ( !strcmp( xname, "perm" ) )
            i = PTK_FB_SORT_BY_PERM;
        else if ( !strcmp( xname, "owner" ) )
            i = PTK_FB_SORT_BY_OWNER;
        else if ( !strcmp( xname, "date" ) )
            i = PTK_FB_SORT_BY_MTIME;
        else if ( !strcmp( xname, "ascend" ) )
        {
            i = -1;
            set->b = browser->sort_type == GTK_SORT_ASCENDING ?  XSET_B_TRUE : XSET_B_FALSE;
        }
        else if ( !strcmp( xname, "descend" ) )
        {
            i = -2;
            set->b = browser->sort_type == GTK_SORT_DESCENDING ?   XSET_B_TRUE : XSET_B_FALSE;
        }
        if ( i > 0 )
            set->b = browser->sort_order == i ? XSET_B_TRUE : XSET_B_FALSE;
        on_popup_sortby( NULL, browser, i );
    }
    else if ( g_str_has_prefix( set->name, "sortx_" ) )
        ptk_file_browser_set_sort_extra( browser, set->name );
    else if ( !strcmp( set->name, "path_help" ) )
        ptk_path_entry_help( NULL, GTK_WIDGET( browser ) );
    else if ( g_str_has_prefix( set->name, "panel" ) )
    {
        i = 0;
        if ( strlen( set->name ) > 6 )
        {
            xname = g_strdup( set->name + 5 );
            xname[1] = '\0';
            i = atoi( xname );
            xname[1] = '_';
            g_free( xname );
        }
        //printf( "ACTION panelN=%d  %c\n", i, set->name[5] );
        if ( i > 0 && i < 5 )
        {
            xname = set->name + 7;     // howdy     value of context offset WAS 7
            if ( !strcmp( xname, "show_hidden" ) )  // shared key
            {
                ptk_file_browser_show_hidden_files( browser, xset_get_b_panel( browser->mypanel, "show_hidden" ) );
            }
            else if ( !strcmp( xname, "show" ) ) // main View|Panel N
                show_panels_all_windows( NULL, (FMMainWindow*)browser->main_window );
            else if ( g_str_has_prefix( xname, "show_" ) )  // shared key
            {
                set2 = xset_get_panel_mode( browser->mypanel, xname, mode );
                set2->b = set2->b == XSET_B_TRUE ? XSET_B_UNSET : XSET_B_TRUE;
                update_views_all_windows( NULL, browser );
                if ( !strcmp( xname, "show_book" ) && browser->side_book )
                {
                    ptk_bookmark_view_chdir( GTK_TREE_VIEW( browser->side_book ), browser, TRUE );
                    gtk_widget_grab_focus( GTK_WIDGET( browser->side_book ) );
                }
            }
            else if ( !strcmp( xname, "list_detailed" ) )  // shared key
                on_popup_list_detailed( NULL, browser );
            else if ( !strcmp( xname, "list_compact" ) )  // shared key
                on_popup_list_compact( NULL, browser );
            else if ( !strcmp( xname, "list_large" ) )  // shared key
            {
                xset_set_b_panel( browser->mypanel, "list_large", !browser->large_icons );
                on_popup_list_large( NULL, browser );

            }
            else if ( !strcmp( xname, "icon_tab" ) || g_str_has_prefix( xname, "font_" ) )
                main_update_fonts( NULL, browser );
            else if ( g_str_has_prefix( xname, "detcol_" )  && browser->view_mode == PTK_FB_LIST_VIEW )
            {
                set2 = xset_get_panel_mode( browser->mypanel, xname, mode );
                set2->b = set2->b == XSET_B_TRUE ? XSET_B_UNSET : XSET_B_TRUE;
                update_views_all_windows( NULL, browser );
            }
            else if ( !strcmp( xname, "icon_status" ) )  // shared key
                on_status_effect_change( NULL, browser );
            else if ( !strcmp( xname, "font_status" ) )  // shared key
                on_status_effect_change( NULL, browser );
        }
    }
    else if ( g_str_has_prefix( set->name, "status_" ) )
    {
        xname = set->name + 7;
        if ( !strcmp( xname, "border" )  ||  !strcmp( xname, "text" ) )
            on_status_effect_change( NULL, browser );
        else if ( !strcmp( xname, "name" )
                    || !strcmp( xname, "path" )
                    || !strcmp( xname, "info" )
                    || !strcmp( xname, "hide" ) )
            on_status_middle_click_config( NULL, set );
    }
    else if ( g_str_has_prefix( set->name, "paste_" ) )
    {
        xname = set->name + 6;
        if ( !strcmp( xname, "link" ) )
            ptk_file_browser_paste_link( browser );
        else if ( !strcmp( xname, "target" ) )
            ptk_file_browser_paste_target( browser );
        else if ( !strcmp( xname, "as" ) )
            ptk_file_misc_paste_as( NULL, browser, ptk_file_browser_get_cwd( browser ), NULL );
    }
    else if ( g_str_has_prefix( set->name, "select_" ) )
    {
        xname = set->name + 7;
        if ( !strcmp( xname, "all" ) )
            ptk_file_browser_select_all( NULL, browser );
        else if ( !strcmp( xname, "un" ) )
            ptk_file_browser_unselect_all( NULL, browser );
        else if ( !strcmp( xname, "invert" ) )
            ptk_file_browser_invert_selection( NULL, browser );
        else if ( !strcmp( xname, "patt" ) )
            ptk_file_browser_select_pattern( NULL, browser, NULL );
    }
    else  // all the rest require ptkfilemenu data
        ptk_file_menu_action( NULL, browser, set->name );
}

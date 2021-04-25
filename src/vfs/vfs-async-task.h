/*
*      vfs-async-task.h
*
* License: See COPYING file
*/

#ifndef __VFS_ASYNC_TASK_H__
#define __VFS_ASYNC_TASK_H__

#include <glib.h>
#include <glib-object.h>

G_BEGIN_DECLS

#define VFS_ASYNC_TASK_TYPE             (vfs_async_task_get_type())
#define VFS_ASYNC_TASK(obj)             (G_TYPE_CHECK_INSTANCE_CAST((obj), VFS_ASYNC_TASK_TYPE, VFSAsyncTask))
#define VFS_ASYNC_TASK_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST((klass),  VFS_ASYNC_TASK_TYPE, VFSAsyncTaskClass))
#define VFS_IS_ASYNC_TASK(obj)              (G_TYPE_CHECK_INSTANCE_TYPE((obj),  VFS_ASYNC_TASK_TYPE))
#define VFS_IS_ASYNC_TASK_CLASS(klass)      (G_TYPE_CHECK_CLASS_TYPE((klass),   VFS_ASYNC_TASK_TYPE))

typedef struct _VFSAsyncTask                VFSAsyncTask;
typedef struct _VFSAsyncTaskClass           VFSAsyncTaskClass;

typedef gpointer (*VFSAsyncFunc)( VFSAsyncTask*, gpointer );

struct _VFSAsyncTask
{
    GObject parent;
    VFSAsyncFunc func;
    gpointer user_data;
    gpointer ret_val;

    GThread* thread;
    GMutex* lock;

    guint idle_id;
    gboolean cancel : 1;
    gboolean cancelled : 1;
    gboolean finished : 1;
};

struct _VFSAsyncTaskClass
{
    GObjectClass parent_class;
    void (*finish)( VFSAsyncTask* task, gboolean is_cancelled );
};

GType       vfs_async_task_get_type (void);
VFSAsyncTask*    vfs_async_task_new          ( VFSAsyncFunc task_func, gpointer user_data );

gpointer vfs_async_task_get_data( VFSAsyncTask* task );
void vfs_async_task_set_data( VFSAsyncTask* task, gpointer user_data  );
gpointer vfs_async_task_get_return_value( VFSAsyncTask* task );

/* Execute the async task */
void vfs_async_task_execute( VFSAsyncTask* task );

gboolean vfs_async_task_is_finished( VFSAsyncTask* task );
gboolean vfs_async_task_is_cancelled( VFSAsyncTask* task );

/*
 * Cancel the async task running in another thread.
 * NOTE: Only can be called from main thread.
 */
void vfs_async_task_cancel( VFSAsyncTask* task );

void vfs_async_task_lock( VFSAsyncTask* task );
void vfs_async_task_unlock( VFSAsyncTask* task );

G_END_DECLS

#endif /* __VFS_ASYNC_TASK_H__ */

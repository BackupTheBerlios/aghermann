// ;-*-C-*- *  Time-stamp: "2010-10-11 11:04:55 hmmr"
/*
 *       File name:  ui/misc.c
 *         Project:  Aghermann
 *          Author:  Andrei Zavada (johnhommer@gmail.com)
 * Initial version:  2010-09-03
 *
 *         Purpose:  misc non-agh specific ui bits
 *
 *         License:  GPL
 */


#include <unistd.h>
#include "misc.h"
#include "ui.h"



void
pop_ok_message( GtkWindow *parent, const gchar *str, ...)
{
	va_list ap;
	va_start (ap, str);

	static GString *buf = NULL;
	if ( buf == NULL )
		buf = g_string_new("");

	g_string_vprintf( buf, str, ap);
	GtkWidget *msg = gtk_message_dialog_new( parent,
						 GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
						 GTK_MESSAGE_INFO,
						 GTK_BUTTONS_OK,
						 buf->str, NULL);
	va_end (ap);

	gtk_dialog_run( GTK_DIALOG (msg));
	gtk_widget_destroy( msg);
}


gint
pop_question( GtkWindow* parent, const gchar *str)
{
	GtkWidget *msg = gtk_message_dialog_new( parent,
						 GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
						 GTK_MESSAGE_INFO,
						 GTK_BUTTONS_YES_NO,
						 str, NULL);
	gint response = gtk_dialog_run( GTK_DIALOG (msg));
	gtk_widget_destroy( msg);
	return response;
}




void
set_cursor_busy( gboolean busy, GtkWidget *wid)
{
	static GdkCursor *cursor_busy   = NULL,
			 *cursor_normal = NULL;
	if ( !cursor_normal ) {
		cursor_busy   = gdk_cursor_new_from_name( gdk_display_get_default(), "wait");
		cursor_normal = gdk_cursor_new_from_name( gdk_display_get_default(), "default");
	}
	if ( busy )
		gdk_window_set_cursor( wid->window, cursor_busy);
	else
		gdk_window_set_cursor( wid->window, cursor_normal);

	while ( gtk_events_pending () )
		gtk_main_iteration();
}





static GtkWidget *wAndNotify;


void
show_and_notify( GtkWindow *relative, const char *fmt, ...)
{
	va_list ap;
	va_start (ap, fmt);

	static GString *buf = NULL;
	if ( buf == NULL )
		buf = g_string_new("");

	g_string_vprintf( buf, fmt, ap);
	va_end (ap);


	if ( wAndNotify )
		gtk_widget_destroy( GTK_WIDGET (wAndNotify));

	wAndNotify = gtk_window_new( GTK_WINDOW_TOPLEVEL);
	gtk_window_set_decorated( GTK_WINDOW (wAndNotify), FALSE);
	gtk_window_set_transient_for( GTK_WINDOW (wAndNotify), relative);
	gtk_window_set_destroy_with_parent( GTK_WINDOW (wAndNotify), TRUE);

	GtkLabel *lAndNotify = gtk_label_new( buf->str);
	gtk_container_set_border_width( GTK_CONTAINER (wAndNotify), 8);
	gtk_container_add( GTK_CONTAINER (wAndNotify), GTK_WIDGET (lAndNotify));

	gint px, py, pw, ph, pw2;
	gtk_window_get_position( relative, &px, &py);
	gtk_window_get_size( relative, &pw, &ph);

	gtk_window_set_default_size( GTK_WINDOW (wAndNotify), -1, -1);
	gtk_widget_queue_resize( wAndNotify);

	gtk_window_get_size( GTK_WINDOW (wAndNotify), &pw2, NULL);
//	printf( "to x = %d, y = %d\n", px+pw/2 - pw2/2, (guint)(py+0.8*ph));
	gtk_window_move( GTK_WINDOW (wAndNotify), px+pw/2 - pw2/2, (guint)(py+0.8*ph));
	gtk_widget_show_all( wAndNotify);
}


void
hide_and_notify()
{
	gtk_widget_destroy( wAndNotify);
	wAndNotify = NULL;
}


gint
agh_ui_construct_misc( GladeXML *xml)
{
//	if ( !(wAndNotify    	  = glade_xml_get_widget( xml, "wAndNotify")) ||
//	     !(lAndNotify    	  = glade_xml_get_widget( xml, "lAndNotify")) )
//		return -1;

	return 0;
}


// EOF
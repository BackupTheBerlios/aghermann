// ;-*-C-*- *  Time-stamp: "2011-03-01 01:05:30 hmmr"
/*
 *       File name:  ui/misc.c
 *         Project:  Aghermann
 *          Author:  Andrei Zavada <johnhommer@gmail.com>
 * Initial version:  2010-09-03
 *
 *         Purpose:  misc non-agh specific ui bits
 *
 *         License:  GPL
 */


#include <math.h>
#include <string.h>
#include "misc.h"
#include "ui.h"


GdkColormap *__cmap;


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
	if ( cursor_normal == NULL) {
		cursor_busy   = gdk_cursor_new_from_name( gdk_display_get_default(), "watch");
		cursor_normal = gdk_cursor_new_from_name( gdk_display_get_default(), "left_ptr");
	}

	gdk_window_set_cursor( wid->window, busy ? cursor_busy : cursor_normal);

	while ( gtk_events_pending () )
		gtk_main_iteration();
}





GdkVisual
	*agh_visual;
GdkColormap
	*agh_cmap;

GString *__ss__;

gint
agh_ui_construct_misc( GladeXML *xml)
{
	__ss__ = g_string_new( "");

      // tell me what they are
	agh_visual = gdk_visual_get_system();
	agh_cmap = gdk_screen_get_default_colormap(
		gdk_screen_get_default());

	return 0;
}




// these are intended for durations, not timestamps
void snprintf_buf_ts_d( double d_)
{
	if ( d_ < 1. )
		snprintf_buf_ts_h( d_ * 24);
	else {
		unsigned m_ = lroundf(d_*24*60*60) / 60,
			m = (m_ % 60),
			h = (m_ / 60) % 24,
			d = (m_ / 60 / 24);
		if ( h % 24 == 0 && m % 60 == 0 )
			snprintf_buf( "%ud", d);
		else if ( m % 60 == 0 )
			snprintf_buf( "%ud%uh", d, h);
		else
			snprintf_buf( "%ud%uh%um", d, h, m);
	}
}

void snprintf_buf_ts_h( double h_)
{
	if ( h_ < 1. )
		snprintf_buf_ts_m( h_ * 60);
	else if ( h_ >= 24. )
		snprintf_buf_ts_d( h_ / 24);
	else {
		unsigned m_ = lroundf( h_*60*60) / 60,
			m = (m_ % 60),
			h = (m_ / 60);
		if ( m % 60 == 0 )
			snprintf_buf( "%uh", h);
		else
			snprintf_buf( "%uh%um", h, m);
	}
}

void snprintf_buf_ts_m( double m_)
{
	if ( m_ < 1. )
		snprintf_buf_ts_s( m_ * 60);
	else if ( m_ >= 60. )
		snprintf_buf_ts_h( m_ / 60);
	else {
		unsigned s_ = lroundf( m_*60) / 60,
			s = (s_ % 60),
			m = (s_ / 60);
		if ( s % 60 == 0 )
			snprintf_buf( "%um", m);
		else
			snprintf_buf( "%um%us", m, s);
	}
}

void snprintf_buf_ts_s( double s_)
{
	if ( s_ >= 60. )
		snprintf_buf_ts_m( s_/60);
	else {
		snprintf_buf( "%.2gs", s_);
	}
}





void
decompose_double( double value, float *mantissa, int *exponent)
{
	static char buf[32];
	snprintf( buf, 31, "%e", value);
	*strchr( buf, 'e') = '|';
	sscanf( buf, "%f|%d", mantissa, exponent);

}

// EOF

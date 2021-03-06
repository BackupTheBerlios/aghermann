// ;-*-C++-*-
/*
 *       File name:  ui/expdesign-admit-one.cc
 *         Project:  Aghermann
 *          Author:  Andrei Zavada <johnhommer@gmail.com>
 * Initial version:  2011-04-18
 *
 *         Purpose:  SExpDesignUI edf import via dnd
 *
 *         License:  GPL
 */


#include "misc.hh"
#include "expdesign.hh"

#if HAVE_CONFIG_H
#  include "config.h"
#endif


using namespace std;
using namespace aghui;



int
aghui::SExpDesignUI::dnd_maybe_admit_one( const char* fname)
{
	agh::CEDFFile *F;
	string info;
	try {
		F = new agh::CEDFFile (fname, ED->fft_params.page_size);
		if ( F->status() & agh::CEDFFile::TStatus::inoperable ) {
			pop_ok_message( wMainWindow, "The header seems to be corrupted in \"%s\"", fname);
			return 0;
		}
		info = F->details();

		snprintf_buf( "File: <i>%s</i>", fname);
		gtk_label_set_markup( lEdfImportCaption, __buf__);
		snprintf_buf( "<b>%s</b>", F->patient.c_str());
		gtk_label_set_markup( lEdfImportSubject, __buf__);

	} catch ( invalid_argument ex) {
		pop_ok_message( (GtkWindow*)wMainWindow, "Could not read edf header in \"%s\"", fname);
		return 0;
	}
	gtk_text_buffer_set_text( tEDFFileDetailsReport, info.c_str(), -1);

      // populate and attach models
	GtkListStore
		*m_groups = gtk_list_store_new( 1, G_TYPE_STRING),
		*m_episodes = gtk_list_store_new( 1, G_TYPE_STRING),
		*m_sessions = gtk_list_store_new( 1, G_TYPE_STRING);
	GtkTreeIter iter;
	for ( auto i = AghGG.begin(); i != AghGG.end(); ++i ) {
		gtk_list_store_append( m_groups, &iter);
		gtk_list_store_set( m_groups, &iter, 0, i->c_str(), -1);
	}
	gtk_combo_box_set_model( eEdfImportGroup,
				 (GtkTreeModel*)m_groups);
	gtk_combo_box_set_entry_text_column( eEdfImportGroup, 0);

	for ( auto i = AghEE.begin(); i != AghEE.end(); ++i ) {
		gtk_list_store_append( m_episodes, &iter);
		gtk_list_store_set( m_episodes, &iter, 0, i->c_str(), -1);
	}
	gtk_combo_box_set_model( eEdfImportEpisode,
				 (GtkTreeModel*)m_episodes);
	gtk_combo_box_set_entry_text_column( eEdfImportEpisode, 0);

	for ( auto i = AghDD.begin(); i != AghDD.end(); ++i ) {
		gtk_list_store_append( m_sessions, &iter);
		gtk_list_store_set( m_sessions, &iter, 0, i->c_str(), -1);
	}
	gtk_combo_box_set_model( eEdfImportSession,
				 (GtkTreeModel*)m_sessions);
	gtk_combo_box_set_entry_text_column( eEdfImportSession, 0);

      // guess episode from fname
	char *fname2 = g_strdup( fname), *episode = strrchr( fname2, '/')+1;
	if ( g_str_has_suffix( episode, ".edf") || g_str_has_suffix( episode, ".EDF") )
		*strrchr( episode, '.') = '\0';
	gtk_entry_set_text( (GtkEntry*)gtk_bin_get_child( (GtkBin*)eEdfImportEpisode),
			    episode);

      // display
	g_signal_emit_by_name( eEdfImportGroupEntry, "changed");

	gint response = gtk_dialog_run( (GtkDialog*)wEdfImport);
	const gchar
		*selected_group   = gtk_entry_get_text( eEdfImportGroupEntry),
		*selected_session = gtk_entry_get_text( eEdfImportSessionEntry),
		*selected_episode = gtk_entry_get_text( eEdfImportEpisodeEntry);
	switch ( response ) {
	case GTK_RESPONSE_OK: // Admit
	{	char *dest_path, *dest, *cmd;
		dest_path = g_strdup_printf( "%s/%s/%s/%s",
					     ED->session_dir(),
					     selected_group,
					     F->patient.c_str(),
					     selected_session);
		dest = g_strdup_printf( "%s/%s.edf",
					dest_path,
					selected_episode);
		if ( gtk_toggle_button_get_active( (GtkToggleButton*)bEdfImportAttachCopy) )
			cmd = g_strdup_printf( "mkdir -p '%s' && cp -n '%s' '%s'", dest_path, fname, dest);
		else if ( gtk_toggle_button_get_active( (GtkToggleButton*)bEdfImportAttachMove) )
			cmd = g_strdup_printf( "mkdir -p '%s' && mv -n '%s' '%s'", dest_path, fname, dest);
		else
			cmd = g_strdup_printf( "mkdir -p '%s' && ln -s '%s' '%s'", dest_path, fname, dest);

		int cmd_exit = system( cmd);
		if ( cmd_exit )
			pop_ok_message( (GtkWindow*)wMainWindow, "Command\n %s\nexited with code %d", cmd_exit);

		g_free( cmd);
		g_free( dest);
		g_free( dest_path);
	}
	    break;
	case GTK_RESPONSE_CANCEL: // Drop
		break;
	case -7: // GTK_RESPONSE_CLOSE:  View separately
		break;
	}

      // finalise
	g_free( fname2);

	g_object_unref( m_groups);
	g_object_unref( m_sessions);
	g_object_unref( m_episodes);

	return 0;
}



extern "C" {

	gboolean
	check_gtk_entry_nonempty( GtkEditable *ignored,
				  // GdkEventKey *event,
				  gpointer  userdata)
	{
		auto& ED = *(SExpDesignUI*)userdata;

		gtk_widget_set_sensitive( (GtkWidget*)ED.bEdfImportAdmit, TRUE);
		gtk_widget_set_sensitive( (GtkWidget*)ED.bEdfImportScoreSeparately, TRUE);

		const gchar *e;
		gchar *ee;

		ee = NULL;
		e = gtk_entry_get_text( ED.eEdfImportGroupEntry);
		if ( !e || !*g_strchug( g_strchomp( ee = g_strdup( e))) ) {
			printf( "e %s\n", e);
			gtk_widget_set_sensitive( (GtkWidget*)ED.bEdfImportAdmit, FALSE);
			gtk_widget_set_sensitive( (GtkWidget*)ED.bEdfImportScoreSeparately, FALSE);
		}
		g_free( ee);

		ee = NULL;
		e = gtk_entry_get_text( ED.eEdfImportSessionEntry);
		if ( !e || !*g_strchug( g_strchomp( ee = g_strdup( e))) ) {
			gtk_widget_set_sensitive( (GtkWidget*)ED.bEdfImportAdmit, FALSE);
			gtk_widget_set_sensitive( (GtkWidget*)ED.bEdfImportScoreSeparately, FALSE);
		}
		g_free( ee);

		ee = NULL;
		e = gtk_entry_get_text( ED.eEdfImportEpisodeEntry);
		if ( !e || !*g_strchug( g_strchomp( ee = g_strdup( e))) ) {
			gtk_widget_set_sensitive( (GtkWidget*)ED.bEdfImportAdmit, FALSE);
			gtk_widget_set_sensitive( (GtkWidget*)ED.bEdfImportScoreSeparately, FALSE);
		}
		g_free( ee);

		gtk_widget_queue_draw( (GtkWidget*)ED.bEdfImportAdmit);
		gtk_widget_queue_draw( (GtkWidget*)ED.bEdfImportScoreSeparately);

		return false;
	}




	gboolean
	cMeasurements_drag_data_received_cb( GtkWidget        *widget,
					     GdkDragContext   *context,
					     gint              x,
					     gint              y,
					     GtkSelectionData *selection_data,
					     guint             info,
					     guint             time,
					     gpointer          userdata)
	{
		auto& ED = *(SExpDesignUI*)userdata;

		gchar **uris = gtk_selection_data_get_uris( selection_data);
		if ( uris != NULL ) {

			guint i = 0;
			while ( uris[i] ) {
				if ( strncmp( uris[i], "file://", 7) == 0 ) {
					char *fname = g_filename_from_uri( uris[i], NULL, NULL);
					int retval = ED.dnd_maybe_admit_one( fname);
					g_free( fname);
					if ( retval )
						break;
				}
				++i;
			}

			// fear no shortcuts
			ED.depopulate( false);
			ED.populate( false);

			g_strfreev( uris);
		}

		gtk_drag_finish (context, TRUE, FALSE, time);
		return TRUE;
	}


	gboolean
	cMeasurements_drag_drop_cb( GtkWidget      *widget,
				    GdkDragContext *context,
				    gint            x,
				    gint            y,
				    guint           time,
				    gpointer        userdata)
	{
		//auto& ED = *(SExpDesignUI*)userdata;
//	GdkAtom         target_type;
//
//      if ( context->targets ) {
//              // Choose the best target type
//              target_type = GDK_POINTER_TO_ATOM
//                      (g_list_nth_data( context->targets, 0));
//		unsigned i = g_list_length(context->targets);
//		while ( i-- )
//			printf( "%zu: %s\n", i, gdk_atom_name( GDK_POINTER_TO_ATOM (g_list_nth_data( context->targets, i))));
//
//		//Request the data from the source.
//              gtk_drag_get_data(
//                      widget,         // will receive 'drag-data-received' signal
//                      context,        // represents the current state of the DnD
//                      target_type,    // the target type we want
//                      time);          // time stamp
//
//	} else { // No target offered by source => error
//              return FALSE;
//	}
//
		return  TRUE;
	}
}

// eof


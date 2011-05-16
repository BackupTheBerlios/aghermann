// ;-*-C++-*- *  Time-stamp: "2011-05-16 01:45:05 hmmr"
/*
 *       File name:  ui/scoring-facility-phasediff.cc
 *         Project:  Aghermann
 *          Author:  Andrei Zavada <johnhommer@gmail.com>
 * Initial version:  2011-01-29
 *
 *         Purpose:  scoring facility phase diff dialog
 *
 *         License:  GPL
 */




#include "scoring-facility.hh"

#if HAVE_CONFIG_H
#  include <config.h>
#endif


using namespace std;

namespace aghui {
namespace sf {

int
SScoringFacility::SPhasediffDialog::construct_widgets()
{
	GtkCellRenderer *renderer;

      // ------- wPhaseDiff
	if ( !(AGH_GBGETOBJ (GtkDialog, wPhaseDiff)) ||
	     !(AGH_GBGETOBJ (GtkDrawingArea, daPhaseDiff)) ||
	     !(AGH_GBGETOBJ (GtkComboBox, ePhaseDiffChannelA)) ||
	     !(AGH_GBGETOBJ (GtkComboBox, ePhaseDiffChannelB)) ||
	     !(AGH_GBGETOBJ (GtkSpinButton, ePhaseDiffFreqFrom)) ||
	     !(AGH_GBGETOBJ (GtkSpinButton, ePhaseDiffFreqUpto)) ||
	     !(AGH_GBGETOBJ (GtkButton, bPhaseDiffApply)) )
		return -1;

	gtk_combo_box_set_model( ePhaseDiffChannelA,
				 (GtkTreeModel*)mEEGChannels);
	renderer = gtk_cell_renderer_text_new();
	gtk_cell_layout_pack_start( (GtkCellLayout*)ePhaseDiffChannelA, renderer, FALSE);
	gtk_cell_layout_set_attributes( (GtkCellLayout*)ePhaseDiffChannelA, renderer,
					"text", 0,
					NULL);
	ePhaseDiffChannelA_changed_cb_handler_id =
		g_signal_connect( ePhaseDiffChannelA, "changed",
				  G_CALLBACK (ePhaseDiffChannelA_changed_cb),
				  this);

	gtk_combo_box_set_model( ePhaseDiffChannelB,
				 (GtkTreeModel*)mEEGChannels);
	renderer = gtk_cell_renderer_text_new();
	gtk_cell_layout_pack_start( (GtkCellLayout*)ePhaseDiffChannelB, renderer, FALSE);
	gtk_cell_layout_set_attributes( (GtkCellLayout*)ePhaseDiffChannelB, renderer,
					"text", 0,
					NULL);
	ePhaseDiffChannelB_changed_cb_handler_id =
		g_signal_connect( ePhaseDiffChannelB, "changed",
				  G_CALLBACK (ePhaseDiffChannelB_changed_cb),
				  this);

	g_signal_connect_after( daPhaseDiff, "expose-event",
				G_CALLBACK (daPhaseDiff_expose_event_cb),
				this);
	g_signal_connect_after( daPhaseDiff, "scroll-event",
				G_CALLBACK (daPhaseDiff_scroll_event_cb),
				this);
	g_signal_connect_after( ePhaseDiffChannelA, "changed",
				G_CALLBACK (ePhaseDiffChannelA_changed_cb),
				this);
	g_signal_connect_after( ePhaseDiffChannelB, "changed",
				G_CALLBACK (ePhaseDiffChannelB_changed_cb),
				this);
	g_signal_connect_after( ePhaseDiffFreqFrom, "value-changed",
				G_CALLBACK (ePhaseDiffFreqFrom_value_changed_cb),
				this);
	g_signal_connect_after( ePhaseDiffFreqUpto, "value-changed",
				G_CALLBACK (ePhaseDiffFreqUpto_value_changed_cb),
				this);
	g_signal_connect_after( bPhaseDiffApply, "clicked",
				G_CALLBACK (bPhaseDiffApply_clicked_cb),
				this);
	g_signal_connect_after( wPhaseDiff, "show",
				G_CALLBACK (wPhaseDiff_show_cb),
				this);
	g_signal_connect_after( wPhaseDiff, "hide",
				G_CALLBACK (wPhaseDiff_hide_cb),
				this);
	return 0;
}



void
SScoringFacility::SPhasediffDialog::update_course()
{
	if ( channel1->samplerate() != channel2->samplerate() )
		return;
	if ( course.size() == 0 )
		course.resize( _parent.total_pages());
//	printf( "ch1 = %s, ch2 = %s, f1 = %g, f2 = %g, order = %u\n", __phasediff_channel1->name, __phasediff_channel2->name, __phasediff_freq_from, __phasediff_freq_upto, __phasediff_order);
	for ( size_t p = 0; p < course.size()-1; ++p )
		course[p] =
			sigproc::phase_diff(
				use_original_signal ? channel1->signal_original : channel1->signal_filtered,
				use_original_signal ? channel2->signal_original : channel2->signal_filtered,
				channel1 -> samplerate(),
				_parent.pagesize() * channel1->samplerate() *  p,
				_parent.pagesize() * channel1->samplerate() * (p+1),
				from, upto,
				bwf_order,
				scope);
}

const SScoringFacility::SChannel*
SScoringFacility::SPhasediffDialog::channel_from_cbox( GtkComboBox *cbox)
{
	GtkTreeIter iter;
	if ( gtk_combo_box_get_active_iter( cbox, &iter) == FALSE )
		return NULL;
	char *entry;
	gtk_tree_model_get( gtk_combo_box_get_model( cbox), &iter,
			    0, &entry,
			    -1);
	for ( auto H = _parent.channels.begin(); H != _parent.channels.end(); ++H )
		if ( strcmp( entry, H->name) == 0 )
			return &*H;
	return NULL;
}


void
SScoringFacility::SPhasediffDialog::preselect_channel( GtkComboBox *cbox, const char *ch)
{
	GtkTreeModel *model = gtk_combo_box_get_model( cbox);
	GtkTreeIter iter;
	gboolean valid;
	valid = gtk_tree_model_get_iter_first( model, &iter);
	while ( valid ) {
		char *entry;
		gtk_tree_model_get( model, &iter,
				    0, &entry,
				    -1);
		if ( strcmp( entry, ch) == 0 ) {
			gtk_combo_box_set_active_iter( cbox, &iter);
			free( entry);
			return;
		}
		free( entry);
		valid = gtk_tree_model_iter_next( model, &iter);
	}
}


namespace phasediff {
int
construct_once()
{
	return 0;
}

}


} // namespace sf


using namespace aghui;
using namespace sf;
using namespace phasediff;

extern "C" {

	gboolean
	daPhaseDiff_expose_event_cb( GtkWidget *wid, GdkEventExpose *event, gpointer userdata)
	{
		auto& PD = *(SScoringFacility::SPhasediffDialog*)userdata;
		GdkWindow *window = gtk_widget_get_window(wid);
		cairo_t *cr = gdk_cairo_create( window);

		gint	ht = gdk_window_get_width( window),
			wd = gdk_window_get_height( window);

		if ( PD.course.size() == 0 ) {
			cairo_show_text( cr, "(uninitialized)");
			cairo_stroke( cr);
			cairo_destroy( cr);
			return TRUE;
		}
		if ( PD.channel1->samplerate() != PD.channel2->samplerate() ) {
			cairo_show_text( cr, "incompatible channels (different samplerate)");
			cairo_stroke( cr);
			cairo_destroy( cr);
			return TRUE;
		}

		// zeroline and hour ticks
		CwB[TColour::ticks_sf].set_source_rgb( cr);
		cairo_set_line_width( cr, 1);
		cairo_move_to( cr, 0,  ht/2);
		cairo_line_to( cr, wd, ht/2);
		cairo_stroke( cr);

		cairo_set_font_size( cr, 10);
		float	hours4 = PD.channel1->recording.F().agh::CHypnogram::length() / 3600. * 4;
		for ( size_t i = 1; i < hours4; ++i ) {
			unsigned tick_pos = (float)i / hours4 * wd;
			cairo_move_to( cr, tick_pos, 0);
			if ( i % 4 == 0 ) {
				cairo_set_line_width( cr, 1.);
			} else if ( i % 2 == 0 )
				cairo_set_line_width( cr, .5);
			else
				cairo_set_line_width( cr, .25);
			cairo_line_to( cr, tick_pos, ht);
			if ( i % 4 == 0 ) {
				snprintf_buf( "%2zuh", i / 4);
				cairo_move_to( cr, tick_pos+5, 12);
				cairo_show_text( cr, __buf__);
			}
			cairo_stroke( cr);
		}

	      // course
		cairo_set_source_rgb( cr, 0., 0., 0.);
		cairo_set_line_width( cr, .5);
		cairo_move_to( cr, 0., PD.course[0] * 1000 * PD.display_scale + ht/2);
		for ( size_t p = 0; p < PD.course.size()-1; ++p )
			cairo_line_to( cr,
				       (float)p/(PD.course.size()-1) * wd,
				       PD.course[p] * 1000 * PD.display_scale + ht/2);
		cairo_stroke( cr);

	      // scale
		cairo_set_line_width( cr, 3.);
		cairo_move_to( cr, 10., 10.);
		cairo_rel_line_to( cr, 0., 1000 * PD.display_scale);
		cairo_move_to( cr, 15, 12);
		cairo_show_text( cr, "1 ms");
		cairo_stroke( cr);

		cairo_destroy( cr);

		return TRUE;
	}


	gboolean
	daPhaseDiff_scroll_event_cb( GtkWidget *wid, GdkEventScroll *event, gpointer userdata)
	{
		auto& PD = *(SScoringFacility::SPhasediffDialog*)userdata;
		switch ( event->direction ) {
		case GDK_SCROLL_UP:
			PD.display_scale *= 1.1;
			break;
		case GDK_SCROLL_DOWN:
			PD.display_scale /= 1.1;
		default:
			break;
		}

		gtk_widget_queue_draw( wid);

		return TRUE;
	}




	void
	ePhaseDiffChannelA_changed_cb( GtkComboBox *cbox, gpointer userdata)
	{
		auto& PD = *(SScoringFacility::SPhasediffDialog*)userdata;
		PD.channel1 = PD.channel_from_cbox( cbox);
		gtk_widget_set_sensitive( (GtkWidget*)PD.bPhaseDiffApply,
					  PD.channel1 != PD.channel2);
	}

	void
	ePhaseDiffChannelB_changed_cb( GtkComboBox *cbox, gpointer userdata)
	{
		auto& PD = *(SScoringFacility::SPhasediffDialog*)userdata;
		PD.channel2 = PD.channel_from_cbox( cbox);
		gtk_widget_set_sensitive( (GtkWidget*)PD.bPhaseDiffApply,
					  PD.channel1 != PD.channel2);
	}




	void
	ePhaseDiffFreqFrom_value_changed_cb( GtkSpinButton *spinbutton,
					     gpointer       userdata)
	{
		auto& PD = *(SScoringFacility::SPhasediffDialog*)userdata;
		PD.from = fabs( gtk_spin_button_get_value( spinbutton) * 10) / 10;
		gtk_widget_set_sensitive( (GtkWidget*)PD.bPhaseDiffApply,
					  PD.from < PD.upto);
	}

	void
	ePhaseDiffFreqUpto_value_changed_cb( GtkSpinButton *spinbutton,
					     gpointer       userdata)
	{
		auto& PD = *(SScoringFacility::SPhasediffDialog*)userdata;
		PD.upto = fabs( gtk_spin_button_get_value( spinbutton) * 10) / 10;
		gtk_widget_set_sensitive( (GtkWidget*)PD.bPhaseDiffApply,
					  PD.from < PD.upto);
	}



	void
	bPhaseDiffApply_clicked_cb( GtkButton *button,
				    gpointer   userdata)
	{
		auto& PD = *(SScoringFacility::SPhasediffDialog*)userdata;
		PD.update_course();
		gtk_widget_queue_draw( (GtkWidget*)PD.daPhaseDiff);
	}



	void
	wPhaseDiff_show_cb( GtkWidget *wid, gpointer userdata)
	{
		auto& PD = *(SScoringFacility::SPhasediffDialog*)userdata;
		if ( gtk_combo_box_get_active( PD.ePhaseDiffChannelA) == -1 ||
		     gtk_combo_box_get_active( PD.ePhaseDiffChannelB) == -1 ) {
			PD.channel1 = &*PD._parent.channels.begin();
			PD.channel2 = &*next(PD._parent.channels.begin());
			PD.preselect_channel( PD.ePhaseDiffChannelA, PD.channel1->name);
			PD.preselect_channel( PD.ePhaseDiffChannelB, PD.channel2->name);
		} else {
			// they have been nicely set, havent't they
			// PD.channel1 = PD.channel_from_cbox( ePhaseDiffChannelA);
			// PD.channel2 = PD.channel_from_cbox( ePhaseDiffChannelB);
		}

		gtk_spin_button_set_value( PD.ePhaseDiffFreqFrom, PD.from);
		gtk_spin_button_set_value( PD.ePhaseDiffFreqUpto, PD.upto);

		gtk_widget_set_sensitive( (GtkWidget*)PD.bPhaseDiffApply,
					  PD.channel1 != PD.channel2 &&
					  PD.from < PD.upto);
	}

	void
	wPhaseDiff_hide_cb( GtkWidget *wid, gpointer userdata)
	{
		auto& PD = *(SScoringFacility::SPhasediffDialog*)userdata;
		gtk_toggle_button_set_active( PD._parent.bScoringFacShowPhaseDiffDialog, FALSE);
	}

} // extern "C"

} // namespace aghui

// eof

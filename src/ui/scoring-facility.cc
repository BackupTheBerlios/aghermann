// ;-*-C++-*- *  Time-stamp: "2011-04-29 04:10:00 hmmr"
/*
 *       File name:  ui/scoring-facility.cc
 *         Project:  Aghermann
 *          Author:  Andrei Zavada <johnhommer@gmail.com>
 * Initial version:  2008-07-01
 *
 *         Purpose:  scoring facility
 *
 *         License:  GPL
 */




//#include <cassert>
//#include <math.h>
//#include <sys/stat.h>

#include <initializer_list>
#include <stdexcept>
#include <fstream>

#include <cairo/cairo-svg.h>
#include <samplerate.h>

#include "libexstrom/exstrom.hh"
#include "misc.hh"
#include "ui.hh"
#include "settings.hh"
#include "scoring-facility.hh"

#if HAVE_CONFIG_H
#  include <config.h>
#endif

using namespace std;

namespace aghui {

// exposed widgets
GtkListStore
	*mScoringPageSize;

GtkWindow
	*wScoringFacility;
GtkComboBox
	*eScoringFacPageSize;
GtkSpinButton
	*eScoringFacCurrentPage;
GtkToggleButton
	*bScoringFacShowFindDialog,
	*bScoringFacShowPhaseDiffDialog;

// other widgets
GtkVBox
	*cScoringFacPageViews;
GtkDrawingArea
	*daScoringFacHypnogram;
GtkButton
	*bScoringFacBack,
	*bScoringFacForward;
GtkToolButton  // there's no reason for these to be different from those two above; just they happen to be toolbuttons in glade
	*bScoreClear, *bScoreNREM1, *bScoreNREM2, *bScoreNREM3, *bScoreNREM4,
	*bScoreREM,   *bScoreWake,  *bScoreMVT,
	*bScoreGotoPrevUnscored, *bScoreGotoNextUnscored;
GtkLabel
	*lScoringFacTotalPages,
	*lScoringFacClockTime,
	*lScoringFacPercentScored,
	*lScoringFacCurrentPos,
	*lScoreStatsNREMPercent,
	*lScoreStatsREMPercent,
	*lScoreStatsWakePercent,
	*lScoringFacCurrentStage,
	*lScoringFacHint;
GtkTable
	*cScoringFacSleepStageStats;
GtkStatusbar
	*sbSF;
GtkMenu
	*mSFPage,
	*mSFPageSelection,
	*mSFPageSelectionInspectChannels,
	*mSFPower,
	*mSFScore,
	*mSFSpectrum;
GtkCheckMenuItem
	*iSFPageShowOriginal,
	*iSFPageShowProcessed,
	*iSFPageShowDZCDF,
	*iSFPageShowEnvelope,
	*iSFAcceptAndTakeNext;
GtkColorButton
	*bColourNONE,
	*bColourNREM1,
	*bColourNREM2,
	*bColourNREM3,
	*bColourNREM4,
	*bColourREM,
	*bColourWake,
	*bColourPowerSF,
	*bColourEMG,
	*bColourHypnogram,
	*bColourArtifacts,
	*bColourTicksSF,
	*bColourLabelsSF,
	*bColourCursor,

	*bColourBandDelta,
	*bColourBandTheta,
	*bColourBandAlpha,
	*bColourBandBeta,
	*bColourBandGamma;




namespace sf {

// saved variables

SGeometry
	GeometryScoringFac;

bool	UseSigAnOnNonEEGChannels = false;

unsigned
	WidgetPageHeight = 130,
	WidgetSpectrumWidth = 100,
	WidgetPowerProfileHeight = 65,
	WidgetEMGProfileHeight = 26;


// module variables

SScoringFacility
	*SF;
size_t	__cur_page_app;

size_t	__pagesize_item = 4;  // pagesize as currently displayed

SChannelPresentation  // for menus & everything else
	*__clicked_channel = NULL;

// general marquee
GtkWidget
	*__marking_in_widget;
double	__marquee_start,
	__marquee_virtual_end;


enum class TUnfazerMode {
	none,
	channel_select,
	calibrate,
};
TUnfazerMode
	__unfazer_mode = TUnfazerMode::none;



inline namespace {

	size_t	__cur_page,
		__cur_pos_hr, __cur_pos_min, __cur_pos_sec;

	float	__sane_signal_display_scale = NAN,
		__sane_power_display_scale = NAN; // 2.5e-5;

	bool __suppress_redraw;


	enum class TTipIdx {
		general,
		unfazer
	};
	const char* const
		__tooltips[] = {
			"<b>Page views:</b>\n"
			"	Wheel:		change signal display scale;\n"
			"	Ctrl+Wheel:	change scale for all channels;\n"
			"	Click2:		reset display scale;\n"
			"  <i>in upper half:</i>\n"
			"	Click1, move, release:	mark artifact;\n"
			"	Click3, move, release:	unmark artifact;\n"
			"  <i>in lower half:</i>\n"
			"	Click3:		context menu.\n"
			"\n"
			"<b>Power profile views:</b>\n"
			"	Click1:	position cursor;\n"
			"	Click2:	draw bands / discrete freq. bins;\n"
			"	Click3:	context menu;\n"
			"	Wheel:	cycle focused band / in-/decrement freq. range;\n"
			"	Shift+Wheel:	in-/decrement scale.\n"
			"\n"
			"<b>Freq. spectrum view:</b>\n"
			"	Click2:	Toggle absolute/relative y-scale;\n"
			"	Wheel:	Scale power (when in abs. mode);\n"
			"	Shift+Wheel:	In-/decrease freq. range.\n"
			"\n"
			"<b>Hypnogram:</b>\n"
			"	Click1:	position cursor;\n"
			"	Click3:	context menu.",

			"<b>Unfazer:</b>\n"
			"	Wheel:		adjust factor;\n"
			"	Click1:		accept;\n"
			"	Click2:		reset factor to 1.;\n"
			"	Ctrl+Click2:	remove unfazer;\n"
			"	Click3:		cancel.\n",
	};

	void repaint_score_stats();
	void do_score_forward();
	bool page_has_artifacts();


guint __crosshair_at;

TScore
	__cur_stage;






} // inline namespace



// struct member functions


// class SChannelPresentation

SChannelPresentation::SChannelPresentation( agh::CRecording& r,
					    SScoringFacility& parent)
      : name (r.channel()),
	type (r.signal_type()),
	recording (r),
	sf (parent),
	_resample_buffer (NULL),
	_resample_buffer_size (0)
{
	get_signal_original();
	get_signal_filtered();

	signal_display_scale =
		isfinite( __sane_signal_display_scale)
		? __sane_signal_display_scale
		: calibrate_display_scale( signal_filtered,
					   APSZ * samplerate() * min (recording.F().length(), (size_t)10),
					   WidgetPageHeight / 2);

	if ( settings::UseSigAnOnNonEEGChannels || strcmp( type, "EEG") == 0 ) {
	      // and signal course
		// snprintf_buf( "(%zu/%zu) %s: low-pass...", h+1, __n_all_channels, HH[h].name);
		// BUF_ON_STATUS_BAR;
		signal_lowpass = SSFLowPassCourse (settings::BWFOrder, settings::BWFCutoff,
						   signal_filtered, samplerate());

	      // and envelope and breadth
		// snprintf_buf( "(%zu/%zu) %s: envelope...", h+1, __n_all_channels, HH[h].name);
		// BUF_ON_STATUS_BAR;
		signal_breadth = SSFEnvelope (settings::EnvTightness,
					      signal_filtered, samplerate());

	      // and dzcdf
		// snprintf_buf( "(%zu/%zu) %s: zerocrossings...", h+1, __n_all_channels, HH[h].name);
		// BUF_ON_STATUS_BAR;
		signal_dzcdf = SSFDzcdf (settings::DZCDFStep,
					 settings::DZCDFSigma,
					 settings::DZCDFSmooth,
					 signal_filtered, samplerate());
	}

      // power and spectrum
	if ( signal_type_is_fftable( type) ) {

		// snprintf_buf( "(%zu/%zu) %s: power...", h+1, __n_all_channels, HH[h].name);
		// BUF_ON_STATUS_BAR;

		// power in a single bin
		power = recording.power_course<float>(
			from = settings::OperatingRangeFrom,
			upto = settings::OperatingRangeUpto);
	      // power spectrum (for the first page)
		n_bins = last_spectrum_bin = recording.n_bins();
		get_spectrum( 0);
		// will be reassigned in REDRAW_ALL
		spectrum_upper_freq = n_bins * recording.binsize();

	      // power in bands
		TBand n_bands = TBand::delta;
		while ( n_bands != TBand::_total )
			if ( settings::FreqBands[(size_t)n_bands][0] >= spectrum_upper_freq )
				break;
			else
				next(n_bands);
		uppermost_band = prev(n_bands);
		get_power_in_bands();

		// delta comes first, calibrate display scale against it
		power_display_scale =
			isfinite( __sane_power_display_scale)
			? __sane_power_display_scale
			: calibrate_display_scale( power_in_bands[(size_t)TBand::delta],
						   power_in_bands[(size_t)TBand::delta].size(),
						   WidgetPageHeight);
	      // switches
		draw_spectrum_absolute = true;
		draw_bands = true;
		focused_band = TBand::delta; // delta
	}

	if ( strcmp( type, "EMG") == 0 ) {
		emg_fabs_per_page.resize( recording.F().agh::CHypnogram::length());
		float largest = 0.;
		size_t i;
		// snprintf_buf( "(%zu/%zu) %s: EMG...", h+1, __n_all_channels, HH[h].name);
		// BUF_ON_STATUS_BAR;
		for ( i = 0; i < emg_fabs_per_page.size(); ++i ) {
			float	current = emg_fabs_per_page[i]
				= abs( valarray<float> (signal_original[ slice (i * PSZ * samplerate(), (i+1) * PSZ * samplerate(), 1) ])).max();
			 if ( largest < current )
				 largest = current;
		 }

		 emg_scale = WidgetEMGProfileHeight/2 / largest;
	}

	percent_dirty = calculate_dirty_percent();

	draw_processed_signal = true;
	draw_original_signal = false;
	draw_dzcdf = draw_envelope = false;


      // widgetz!

      // expander and vbox
	gchar *h_escaped = g_markup_escape_text( name, -1);
	snprintf_buf( "%s <b>%s</b>", type, h_escaped);
	g_free( h_escaped);
	expander = (GtkExpander*) gtk_expander_new( __buf__);
	gtk_expander_set_use_markup( expander, TRUE);

	gtk_box_pack_start( (GtkBox*)cScoringFacPageViews,
			    (GtkWidget*)expander, TRUE, TRUE, 0);
	gtk_expander_set_expanded( expander,
				   TRUE);
	gtk_container_add( (GtkContainer*)expander,
			   (GtkWidget*)(vbox = (GtkVBox*) (gtk_vbox_new( FALSE, 0))));

      // page view
	gtk_container_add( (GtkContainer*)vbox,
			   (GtkWidget*) (da_page = (GtkDrawingArea*) (gtk_drawing_area_new())));
	g_object_set( G_OBJECT (da_page),
		      "app-paintable", TRUE,
		      "height-request", WidgetPageHeight,
		      NULL);
	g_signal_connect_after( da_page, "expose-event",
				G_CALLBACK (daScoringFacPageView_expose_event_cb),
				(gpointer)this);
	g_signal_connect_after( da_page, "button-press-event",
				G_CALLBACK (daScoringFacPageView_button_press_event_cb),
				(gpointer)this);
	g_signal_connect_after( da_page, "button-release-event",
				G_CALLBACK (daScoringFacPageView_button_release_event_cb),
				(gpointer)this);
	g_signal_connect_after( da_page, "motion-notify-event",
				G_CALLBACK (daScoringFacPageView_motion_notify_event_cb),
				(gpointer)this);
	g_signal_connect_after( da_page, "scroll-event",
				G_CALLBACK (daScoringFacPageView_scroll_event_cb),
				(gpointer)this);
	gtk_widget_add_events( (GtkWidget*)da_page,
			       (GdkEventMask)
			       GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK |
			       GDK_KEY_PRESS_MASK | GDK_POINTER_MOTION_MASK | GDK_DRAG_MOTION);

	if ( signal_type_is_fftable( type) ) {
	      // power pane
		GtkWidget *hbox;
		gtk_container_add( (GtkContainer*)vbox,
				   hbox = gtk_hbox_new( FALSE, 0));
		gtk_container_add( (GtkContainer*)hbox,
				   (GtkWidget*) (da_power = (GtkDrawingArea*) (gtk_drawing_area_new())));
		gtk_container_add_with_properties( (GtkContainer*) (hbox),
						   (GtkWidget*) (da_spectrum = (GtkDrawingArea*) (gtk_drawing_area_new())),
						   "expand", FALSE,
						   NULL);
		g_object_set( G_OBJECT (da_power),
			      "app-paintable", TRUE,
			      "height-request", WidgetPowerProfileHeight,
			      NULL);
		g_signal_connect_after( da_power, "expose-event",
					G_CALLBACK (daScoringFacPSDProfileView_expose_event_cb),
					(gpointer)this);
		g_signal_connect_after( da_power, "button-press-event",
					G_CALLBACK (daScoringFacPSDProfileView_button_press_event_cb),
					(gpointer)this);
		g_signal_connect_after( da_power, "scroll-event",
					G_CALLBACK (daScoringFacPSDProfileView_scroll_event_cb),
					(gpointer)this);
		gtk_widget_add_events( (GtkWidget*)da_power,
				       (GdkEventMask) GDK_BUTTON_PRESS_MASK);

	      // spectrum pane
		g_object_set( G_OBJECT (da_spectrum),
			      "app-paintable", TRUE,
			      "width-request", WidgetSpectrumWidth,
			      NULL);
		// gtk_widget_modify_fg( da_spectrum, GTK_STATE_NORMAL, &__fg1__[cSPECTRUM]);
		// gtk_widget_modify_bg( da_spectrum, GTK_STATE_NORMAL, &__bg1__[cSPECTRUM]);

		g_signal_connect_after( da_spectrum, "expose-event",
					G_CALLBACK (daScoringFacSpectrumView_expose_event_cb),
					(gpointer)this);
		g_signal_connect_after( da_spectrum, "button-press-event",
					G_CALLBACK (daScoringFacSpectrumView_button_press_event_cb),
					(gpointer)this);
		g_signal_connect_after( da_spectrum, "scroll-event",
					G_CALLBACK (daScoringFacSpectrumView_scroll_event_cb),
					(gpointer)this);
		gtk_widget_add_events( (GtkWidget*)da_spectrum, (GdkEventMask) GDK_BUTTON_PRESS_MASK);

	} else
		da_power = da_spectrum = NULL;

	if ( strcmp( type, "EMG") == 0 ) {
		 GtkWidget *hbox, *da_void;
		 gtk_container_add( (GtkContainer*)vbox,
				    hbox = gtk_hbox_new( FALSE, 0));
		 gtk_container_add( (GtkContainer*) (hbox),
				    (GtkWidget*) (da_emg_profile = (GtkDrawingArea*) (gtk_drawing_area_new())));
		 gtk_container_add_with_properties( (GtkContainer*)hbox,
						    da_void = gtk_drawing_area_new(),
						    "expand", FALSE,
						    NULL);
		 g_object_set( (GObject*)da_emg_profile,
			       "app-paintable", TRUE,
			       "height-request", WidgetEMGProfileHeight,
			       NULL);
		 g_object_set( (GObject*)da_void,
			       "width-request", WidgetSpectrumWidth,
			       NULL);
		 g_signal_connect_after( da_emg_profile, "expose-event",
					 G_CALLBACK (daScoringFacEMGProfileView_expose_event_cb),
					 (gpointer)this);
		 g_signal_connect_after( da_emg_profile, "button-press-event",
					 G_CALLBACK (daScoringFacEMGProfileView_button_press_event_cb),
					 (gpointer)this);
		 g_signal_connect_after( da_emg_profile, "scroll-event",
					 G_CALLBACK (daScoringFacEMGProfileView_scroll_event_cb),
					 (gpointer)this);
		 gtk_widget_add_events( (GtkWidget*)da_emg_profile,
					(GdkEventMask) GDK_BUTTON_PRESS_MASK);
	 } else {
		 da_emg_profile = NULL;
	 }

      // // add channel under mSFPageSelectionInspectChannels
      // 	gtk_container_add( GTK_CONTAINER (mSFPageSelectionInspectChannels),
      // 			   GTK_WIDGET (menu_item = GTK_MENU_ITEM (gtk_check_menu_item_new_with_label( name))));
      // 	g_object_set( G_OBJECT (menu_item),
      // 		      "visible", TRUE,
      // 		      NULL);
}

SChannelPresentation::~SChannelPresentation()
{
	free( (void*)_resample_buffer);
}


float
SChannelPresentation::calibrate_display_scale( const valarray<float>& signal,
					       size_t over, float fit)
{
	float max_over = 0.;
	for ( size_t i = 0; i < over; ++i )
		if ( max_over < signal[i] )
			max_over = signal[i];
	return fit / max_over;
}


float
SChannelPresentation::calculate_dirty_percent()
{
	size_t total = 0; // in samples
	auto& af = recording.F()[name].artifacts;
	for_each( af.begin(), af.end(),
		  [&total] ( const agh::CEDFFile::SSignal::TRegion& r)
		  {
			  total += r.second - r.first;
		  });
	return percent_dirty = (float)total / n_samples();
}




void
SChannelPresentation::mark_region_as_artifact( size_t start, size_t end, bool do_mark)
{
	if ( do_mark )
		recording.F()[name].mark_artifact( start, end);
	else
		recording.F()[name].clear_artifact( start, end);

	calculate_dirty_percent();

	get_signal_filtered();

	if ( have_power() ) {
		get_power();
		get_power_in_bands();
		get_spectrum( __cur_page);

		gtk_widget_queue_draw( (GtkWidget*)da_power);
		gtk_widget_queue_draw( (GtkWidget*)da_spectrum);
	}
	gtk_widget_queue_draw( (GtkWidget*)da_page);
}



void
SChannelPresentation::draw_signal( const valarray<float>& signal,
				   unsigned width, int vdisp, cairo_t *cr)
{
	size_t samples_per_page = APSZ * samplerate();
	draw_signal( signal,
		     (__cur_page_app + 0) * samples_per_page,
		     (__cur_page_app + 1) * samples_per_page,
		     width, vdisp, cr);
}



void
SChannelPresentation::draw_signal( const valarray<float>& signal,
				   size_t start, size_t end,
				   unsigned width, int vdisp,
				   cairo_t *cr)
{
	if ( use_resample ) {
		if ( _resample_buffer_size != width )
			_resample_buffer = (float*)realloc( _resample_buffer,
							    (_resample_buffer_size = width) * sizeof(float));
		SRC_DATA samples;
		samples.data_in      = const_cast<float*>(&signal[start]);
		samples.input_frames = end - start;
		samples.data_out     = _resample_buffer;
		samples.src_ratio    = (double)samples.output_frames / samples.input_frames;

		if ( src_simple( &samples, SRC_SINC_FASTEST /*SRC_LINEAR*/, 1) )
			;

		size_t i;
		cairo_move_to( cr, 0,
			       - samples.data_out[0]
			       * signal_display_scale
			       + vdisp);
		for ( i = 0; i < width; ++i )
			cairo_line_to( cr, i,
				       - samples.data_out[i]
				       * signal_display_scale
				       + vdisp);

		free( (void*)samples.data_out);

	} else {
		size_t i;
		cairo_move_to( cr, 0,
			       - signal[ __cur_page_app * APSZ * samplerate() ]
			       * signal_display_scale
			       + vdisp);
		size_t length = end - start;
		for ( i = 0; i < length; ++i ) {
			cairo_line_to( cr, ((float)i)/length * width,
				       - signal[ start + i ]
				       * signal_display_scale
				       + vdisp);
		}
	}
}





// class SScoringFacility


SScoringFacility::SScoringFacility( agh::CSubject& J,
				    const string& D, const string& E)
{
	set_cursor_busy( true, (GtkWidget*)wMainWindow);

      // get display scales
	{
		ifstream ifs (make_fname__common( channels.front().recording.F().filename(), true) + ".displayscale");
		if ( not ifs.good() ||
		     (ifs >> __sane_signal_display_scale >> __sane_power_display_scale, ifs.gcount() == 0) )
			__sane_signal_display_scale = __sane_power_display_scale = NAN;
	}
	// sane values, now set, will be used in SChannelPresentation ctors

      // iterate all of AghHH, mark our channels
	for ( auto H = AghHH.begin(); H != AghHH.end(); ++H ) {
		snprintf_buf( "Reading and processing channel %s...", H->c_str());
		buf_on_status_bar();
		try {
			channels.emplace_back( J.measurements.at(D)[E].recordings.at(*H), *this);
		} catch (...) {
		}
	}

	if ( channels.size() == 0 )
		throw invalid_argument( string("no channels found for combination (") + J.name() + ", " + D + ", " + E + ")");

      // histogram -> scores
	const CEDFFile& F = channels.begin()->recording.F();
	hypnogram.resize( F.agh::CHypnogram::length());
	for ( size_t p = 0; p < F.CHypnogram::length(); ++p )
		hypnogram[p] = F.nth_page(p).score_code();

      // count n_eeg_channels
	n_eeg_channels =
		count_if( channels.begin(), channels.end(),
			  [] (const SChannelPresentation& h)
			  {
				  return strcmp( h.type, "EEG") == 0;
			  });

       // // finish mSFPageSelectionInspectChannels
       // 	GtkWidget *iSFPageSelectionInspectMany = gtk_menu_item_new_with_label( "Inspect these");
       // 	gtk_container_add( GTK_CONTAINER (mSFPageSelectionInspectChannels),
       // 			   iSFPageSelectionInspectMany);
       // 	g_object_set( (GObject*)(iSFPageSelectionInspectMany),
       // 		      "visible", TRUE,
       // 		      NULL);
       // 	g_signal_connect_after( iSFPageSelectionInspectMany, "select",  // but why the hell not "activate"?? GTK+ <3<3<#<#,3,3
       // 				G_CALLBACK (iSFPageSelectionInspectMany_activate_cb),
       // 				NULL);

       // recalculate (average) signal and power display scales
	if ( isfinite( __sane_signal_display_scale) ) {
		;  // we've got it saved previously
	} else {
		__sane_signal_display_scale = __sane_power_display_scale = 0.;
		size_t n_with_power = 0;
		for ( auto h = channels.begin(); h != channels.end(); ++h ) {
			__sane_signal_display_scale += h->signal_display_scale;
			if ( h->have_power() ) {
				++n_with_power;
				__sane_power_display_scale += h->power_display_scale;
			}
		}
		__sane_signal_display_scale /= channels.size();
		__sane_power_display_scale /= n_with_power;
		for ( auto h = channels.begin(); h != channels.end(); ++h ) {
			h->signal_display_scale = __sane_signal_display_scale;
			if ( h->have_power() )
				h->power_display_scale = __sane_power_display_scale;
		}
	}


      // set up other controls
	// set window title
	snprintf_buf( "Scoring: %s\342\200\231s %s in %s",
		      J.name(), E.c_str(), D.c_str());
	gtk_window_set_title( (GtkWindow*)wScoringFacility,
			      __buf__);

	// assign tooltip
	gtk_widget_set_tooltip_markup( (GtkWidget*)(lScoringFacHint), __tooltips[(size_t)TTipIdx::general]);

	// align empty area next to EMG profile with spectrum panes vertically
	g_object_set( (GObject*)(cScoringFacSleepStageStats),
		      "width-request", WidgetSpectrumWidth,
		      NULL);

	// grey out phasediff button if there are fewer than 2 EEG channels
	gtk_widget_set_sensitive( (GtkWidget*)(bScoringFacShowPhaseDiffDialog), (n_eeg_channels >= 2));

	// desensitize iSFAcceptAndTakeNext unless there are more episodes
	gtk_widget_set_sensitive( (GtkWidget*)(iSFAcceptAndTakeNext),
				  J.measurements.at(D).episodes.back().name() != E);

	// draw all
	__suppress_redraw = true;
	__cur_page_app = __cur_page = 0;
	gtk_combo_box_set_active( (GtkComboBox*)(eScoringFacPageSize),
				  pagesize_is_right());

	gtk_spin_button_set_value( eScoringFacCurrentPage,
				   1);
	__suppress_redraw = false;
	g_signal_emit_by_name( eScoringFacPageSize, "changed");
	//	gtk_widget_queue_draw( cMeasurements);

	gtk_statusbar_pop( sbMainStatusBar, sbContextIdGeneral);
	set_cursor_busy( false, (GtkWidget*)(wMainWindow));

	calculate_scored_percent();
	repaint_score_stats();
}


SScoringFacility::~SScoringFacility()
{
	// save display scales
	{
		ofstream ofs (make_fname__common( channels.front().recording.F().filename(), true) + ".displayscale");
		if ( ofs.good() )
			ofs << __sane_signal_display_scale << __sane_power_display_scale;
	}

	// destroy widgets
	gtk_container_foreach( GTK_CONTAINER (cScoringFacPageViews),
			       (GtkCallback) gtk_widget_destroy,
			       NULL);
	gtk_container_foreach( GTK_CONTAINER (mSFPageSelectionInspectChannels),
			       (GtkCallback) gtk_widget_destroy,
			       NULL);
}

void
SScoringFacility::queue_redraw_all() const
{
	 for ( auto H = channels.begin(); H != channels.end(); ++H )
		 if ( H->have_power() ) {
			 gtk_widget_queue_draw( GTK_WIDGET (H->da_power));
			 gtk_widget_queue_draw( GTK_WIDGET (H->da_spectrum));
		 }
}





// functions

int
construct( GtkBuilder *builder)
{
	 GtkCellRenderer *renderer;

	 if ( !(AGH_GBGETOBJ (builder, GtkWindow,	wScoringFacility)) ||
	      !(AGH_GBGETOBJ (builder, GtkComboBox,	eScoringFacPageSize)) ||
	      !(AGH_GBGETOBJ (builder, GtkVBox,		cScoringFacPageViews)) ||
	      !(AGH_GBGETOBJ (builder, GtkDrawingArea,	daScoringFacHypnogram)) ||
	      !(AGH_GBGETOBJ (builder, GtkButton,	bScoringFacBack)) ||
	      !(AGH_GBGETOBJ (builder, GtkButton,	bScoringFacForward)) ||
	      !(AGH_GBGETOBJ (builder, GtkLabel,		lScoringFacTotalPages)) ||
	      !(AGH_GBGETOBJ (builder, GtkSpinButton,	eScoringFacCurrentPage)) ||
	      !(AGH_GBGETOBJ (builder, GtkLabel,		lScoringFacClockTime)) ||
	      !(AGH_GBGETOBJ (builder, GtkLabel,		lScoringFacCurrentStage)) ||
	      !(AGH_GBGETOBJ (builder, GtkLabel,		lScoringFacCurrentPos)) ||
	      !(AGH_GBGETOBJ (builder, GtkLabel,		lScoringFacPercentScored)) ||
	      !(AGH_GBGETOBJ (builder, GtkLabel,		lScoreStatsNREMPercent)) ||
	      !(AGH_GBGETOBJ (builder, GtkLabel,		lScoreStatsREMPercent)) ||
	      !(AGH_GBGETOBJ (builder, GtkLabel,		lScoreStatsWakePercent)) ||
	      !(AGH_GBGETOBJ (builder, GtkToolButton,	bScoreClear)) ||
	      !(AGH_GBGETOBJ (builder, GtkToolButton,	bScoreNREM1)) ||
	      !(AGH_GBGETOBJ (builder, GtkToolButton,	bScoreNREM2)) ||
	      !(AGH_GBGETOBJ (builder, GtkToolButton,	bScoreNREM3)) ||
	      !(AGH_GBGETOBJ (builder, GtkToolButton,	bScoreNREM4)) ||
	      !(AGH_GBGETOBJ (builder, GtkToolButton,	bScoreREM))   ||
	      !(AGH_GBGETOBJ (builder, GtkToolButton,	bScoreWake))  ||
	      !(AGH_GBGETOBJ (builder, GtkToolButton,	bScoreMVT))   ||
	      !(AGH_GBGETOBJ (builder, GtkToolButton,	bScoreGotoPrevUnscored)) ||
	      !(AGH_GBGETOBJ (builder, GtkToolButton,	bScoreGotoNextUnscored)) ||
	      !(AGH_GBGETOBJ (builder, GtkToggleButton,	bScoringFacShowFindDialog)) ||
	      !(AGH_GBGETOBJ (builder, GtkToggleButton,	bScoringFacShowPhaseDiffDialog)) ||
	      !(AGH_GBGETOBJ (builder, GtkTable,		cScoringFacSleepStageStats)) ||
	      !(AGH_GBGETOBJ (builder, GtkLabel,		lScoringFacHint)) ||
	      !(AGH_GBGETOBJ (builder, GtkStatusbar,	sbSF)) )
		 return -1;

	 gtk_combo_box_set_model( (GtkComboBox*)(eScoringFacPageSize),
				  (GtkTreeModel*)(mScoringPageSize));

	 renderer = gtk_cell_renderer_text_new();
	 gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (eScoringFacPageSize), renderer, FALSE);
	 gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (eScoringFacPageSize), renderer,
					 "text", 0,
					 NULL);

      // ------- menus
	 if ( !(AGH_GBGETOBJ (builder, GtkMenu, mSFPage)) ||
	      !(AGH_GBGETOBJ (builder, GtkMenu, mSFPageSelection)) ||
	      !(AGH_GBGETOBJ (builder, GtkMenu, mSFPageSelectionInspectChannels)) ||
	      !(AGH_GBGETOBJ (builder, GtkMenu, mSFPower)) ||
	      !(AGH_GBGETOBJ (builder, GtkMenu, mSFScore)) ||
	      !(AGH_GBGETOBJ (builder, GtkMenu, mSFSpectrum)) ||
	      !(AGH_GBGETOBJ (builder, GtkCheckMenuItem, iSFPageShowOriginal)) ||
	      !(AGH_GBGETOBJ (builder, GtkCheckMenuItem, iSFPageShowProcessed)) ||
	      !(AGH_GBGETOBJ (builder, GtkCheckMenuItem, iSFPageShowDZCDF)) ||
	      !(AGH_GBGETOBJ (builder, GtkCheckMenuItem, iSFPageShowEnvelope)) ||
	      !(AGH_GBGETOBJ (builder, GtkCheckMenuItem, iSFAcceptAndTakeNext)) )
		 return -1;

      // ------ colours
	 if ( !(AGH_GBGETOBJ (builder, GtkColorButton, bColourNONE)) ||
	      !(AGH_GBGETOBJ (builder, GtkColorButton, bColourNREM1)) ||
	      !(AGH_GBGETOBJ (builder, GtkColorButton, bColourNREM2)) ||
	      !(AGH_GBGETOBJ (builder, GtkColorButton, bColourNREM3)) ||
	      !(AGH_GBGETOBJ (builder, GtkColorButton, bColourNREM4)) ||
	      !(AGH_GBGETOBJ (builder, GtkColorButton, bColourREM)) ||
	      !(AGH_GBGETOBJ (builder, GtkColorButton, bColourWake)) ||
	      !(AGH_GBGETOBJ (builder, GtkColorButton, bColourPowerSF)) ||
	      !(AGH_GBGETOBJ (builder, GtkColorButton, bColourEMG)) ||
	      !(AGH_GBGETOBJ (builder, GtkColorButton, bColourHypnogram)) ||
	      !(AGH_GBGETOBJ (builder, GtkColorButton, bColourArtifacts)) ||
	      !(AGH_GBGETOBJ (builder, GtkColorButton, bColourTicksSF)) ||
	      !(AGH_GBGETOBJ (builder, GtkColorButton, bColourLabelsSF)) ||
	      !(AGH_GBGETOBJ (builder, GtkColorButton, bColourCursor)) ||
	      !(AGH_GBGETOBJ (builder, GtkColorButton, bColourBandDelta)) ||
	      !(AGH_GBGETOBJ (builder, GtkColorButton, bColourBandTheta)) ||
	      !(AGH_GBGETOBJ (builder, GtkColorButton, bColourBandAlpha)) ||
	      !(AGH_GBGETOBJ (builder, GtkColorButton, bColourBandBeta)) ||
	      !(AGH_GBGETOBJ (builder, GtkColorButton, bColourBandGamma)) )
		 return -1;

	 return 0;
}

void
destruct()
{
}




size_t
marquee_to_az()
{
	int wd = gdk_window_get_width( gtk_widget_get_window( GTK_WIDGET (__clicked_channel->da_page)));
	float	x1 = __marquee_start,
		x2 = __marquee_virtual_end;
	if ( x1 > x2 ) { float _ = x1; x1 = x2, x2 = _; }
	if ( x1 < 0. ) x1 = 0.;

	__pattern_ia = (__cur_page_app + x1/wd) * APSZ * __clicked_channel->samplerate();
	__pattern_iz = (__cur_page_app + x2/wd) * APSZ * __clicked_channel->samplerate();
	if ( __pattern_ia > __clicked_channel->n_samples() )
		return 0;
	if ( __pattern_iz > __clicked_channel->n_samples() )
		__pattern_iz = __clicked_channel->n_samples();

	__pattern_wd = (float)(__pattern_iz - __pattern_ia)/(__clicked_channel->samplerate() * APSZ) * wd;

	return (__pattern_iz - __pattern_ia);
}



} // namespace sf



using namespace sf;

inline namespace {

	void
	repaint_score_stats()
	{
		snprintf_buf( "<b>%3.1f</b> %% scored", SF->scored_percent);
		gtk_label_set_markup( GTK_LABEL (lScoringFacPercentScored), __buf__);

		snprintf_buf( "<small>%3.1f</small> %%", SF->scored_percent_nrem);
		gtk_label_set_markup( GTK_LABEL (lScoreStatsNREMPercent), __buf__);

		snprintf_buf( "<small>%3.1f</small> %%", SF->scored_percent_rem);
		gtk_label_set_markup( GTK_LABEL (lScoreStatsREMPercent), __buf__);

		snprintf_buf( "<small>%3.1f</small> %%", SF->scored_percent_wake);
		gtk_label_set_markup( GTK_LABEL (lScoreStatsWakePercent), __buf__);
	}

	void
	do_score_forward( char score_ch)
	{
		if ( __cur_page < SF->total_pages() ) {
			SF->hypnogram[__cur_page] = score_ch;
			++__cur_page;
			++__cur_page_app; //  = P2AP (__cur_page);
			gtk_spin_button_set_value( eScoringFacCurrentPage, __cur_page_app+1);
			repaint_score_stats();
		}
	}


	bool
	page_has_artifacts( size_t p)
	{
		for ( auto H = SF->channels.begin(); H != SF->channels.end(); ++H ) {
			auto& Aa = H->recording.F()[H->name].artifacts;
			auto spp = APSZ * H->samplerate();
			if ( any_of( Aa.begin(), Aa.end(),
				     [&] (const agh::CEDFFile::SSignal::TRegion& span)
				     {
					     return ( (p * spp < span.first &&
						       span.first < (p+1) * spp) ||
						      (p * spp < span.second &&
						       span.second < (p+1) * spp)
						      ||
						      (span.first < p * spp &&
						       (p+1) * spp < span.second) );
				     }) )
				return true;
		}
		return false;
	}
}





// callbaaaackz!

extern "C" {



// ---------- page value_changed


	void
	eScoringFacPageSize_changed_cb()
	{
		auto cur_pos = __cur_page_app * APSZ;
		__pagesize_item = gtk_combo_box_get_active( eScoringFacPageSize);
		__cur_page_app = cur_pos / APSZ;
//	__cur_page = AP2P (__cur_page_app); // shouldn't change

		gtk_spin_button_set_range( eScoringFacCurrentPage, 1, p2ap( SF->total_pages()));
		gtk_spin_button_set_value( eScoringFacCurrentPage, __cur_page_app+1);

		snprintf_buf( "<small>of</small> %zu", p2ap(SF->total_pages()));
		gtk_label_set_markup( lScoringFacTotalPages, __buf__);

		gtk_widget_set_sensitive( (GtkWidget*)(bScoreClear), pagesize_is_right());
		gtk_widget_set_sensitive( (GtkWidget*)(bScoreNREM1), pagesize_is_right());
		gtk_widget_set_sensitive( (GtkWidget*)(bScoreNREM2), pagesize_is_right());
		gtk_widget_set_sensitive( (GtkWidget*)(bScoreNREM3), pagesize_is_right());
		gtk_widget_set_sensitive( (GtkWidget*)(bScoreNREM4), pagesize_is_right());
		gtk_widget_set_sensitive( (GtkWidget*)(bScoreREM),   pagesize_is_right());
		gtk_widget_set_sensitive( (GtkWidget*)(bScoreWake),  pagesize_is_right());
		gtk_widget_set_sensitive( (GtkWidget*)(bScoreMVT),   pagesize_is_right());
		gtk_widget_set_sensitive( (GtkWidget*)(bScoreGotoPrevUnscored), pagesize_is_right());
		gtk_widget_set_sensitive( (GtkWidget*)(bScoreGotoNextUnscored), pagesize_is_right());

		if ( !__suppress_redraw )
			SF->queue_redraw_all();
	}





	void
	eScoringFacCurrentPage_value_changed_cb()
	{
		__cur_page_app = gtk_spin_button_get_value( eScoringFacCurrentPage) - 1;
		__cur_page = ap2p( __cur_page_app);
		auto __cur_pos = __cur_page_app * APSZ;
		__cur_pos_hr  =  __cur_pos / 3600;
		__cur_pos_min = (__cur_pos - __cur_pos_hr * 3600) / 60;
		__cur_pos_sec =  __cur_pos % 60;

		__cur_stage = agh::SPage::char2score( SF->hypnogram[__cur_pos / PSZ]);

		for ( auto H = SF->channels.begin(); H != SF->channels.end(); ++H )
			if ( gtk_expander_get_expanded(H->expander) && H->da_page )
				if ( !__suppress_redraw ) {
					gtk_widget_queue_draw( (GtkWidget*)H->da_page);

					if ( H->have_power() ) {
						H->spectrum = H->recording.power_spectrum<float>( __cur_page);
						gtk_widget_queue_draw( (GtkWidget*) H->da_spectrum);
						gtk_widget_queue_draw( (GtkWidget*) H->da_power);
					}
					if ( H->da_emg_profile )
						gtk_widget_queue_draw( (GtkWidget*) H->da_emg_profile);
				}

		snprintf_buf( "<b><big>%s</big></b>", agh::SPage::score_name(__cur_stage));
		gtk_label_set_markup( lScoringFacCurrentStage, __buf__);

		snprintf_buf( "<b>%2zu:%02zu:%02zu</b>", __cur_pos_hr, __cur_pos_min, __cur_pos_sec);
		gtk_label_set_markup( lScoringFacCurrentPos, __buf__);

		time_t time_at_cur_pos = SF->start_time() + __cur_pos;
		char tmp[10];
		strftime( tmp, 9, "%H:%M:%S", localtime( &time_at_cur_pos));
		snprintf_buf( "<b>%s</b>", tmp);
		gtk_label_set_markup( lScoringFacClockTime, __buf__);

		if ( !__suppress_redraw )
			gtk_widget_queue_draw( (GtkWidget*)daScoringFacHypnogram);
	}








// -------------- various buttons






	void bScoreClear_clicked_cb()  { do_score_forward( agh::SPage::score_code(TScore::none)); }
	void bScoreNREM1_clicked_cb()  { do_score_forward( agh::SPage::score_code(TScore::nrem1)); }
	void bScoreNREM2_clicked_cb()  { do_score_forward( agh::SPage::score_code(TScore::nrem2)); }
	void bScoreNREM3_clicked_cb()  { do_score_forward( agh::SPage::score_code(TScore::nrem3)); }
	void bScoreNREM4_clicked_cb()  { do_score_forward( agh::SPage::score_code(TScore::nrem4)); }
	void bScoreREM_clicked_cb()    { do_score_forward( agh::SPage::score_code(TScore::rem)); }
	void bScoreWake_clicked_cb()   { do_score_forward( agh::SPage::score_code(TScore::wake)); }
	void bScoreMVT_clicked_cb()    { do_score_forward( agh::SPage::score_code(TScore::mvt)); }





	void
	bScoringFacForward_clicked_cb()
	{
		if ( __cur_page_app < p2ap( SF->total_pages()) ) {
			++__cur_page_app;
			__cur_page = ap2p(__cur_page_app);
			gtk_spin_button_set_value( eScoringFacCurrentPage, __cur_page_app+1);
		}
	}

	void
	bScoringFacBack_clicked_cb()
	{
		if ( __cur_page_app ) {
			--__cur_page_app;
			__cur_page = ap2p(__cur_page_app);
			gtk_spin_button_set_value( eScoringFacCurrentPage, __cur_page_app+1);
		}
	}




	void
	bScoreGotoPrevUnscored_clicked_cb()
	{
		if ( not pagesize_is_right() || __cur_page == 0 )
			return;
		size_t p = __cur_page - 1;
		while ( SF->hypnogram[p] != agh::SPage::score_code(TScore::none) )
			if ( p > 0 )
				--p;
			else
				break;
		gtk_spin_button_set_value( eScoringFacCurrentPage,
					   (__cur_page_app = __cur_page = p)+1);
	}

	void
	bScoreGotoNextUnscored_clicked_cb()
	{
		if ( not pagesize_is_right() || __cur_page == SF->total_pages() )
			return;
		size_t p = __cur_page + 1;
		while ( SF->hypnogram[p] != agh::SPage::score_code(TScore::none) )
			if ( p < SF->total_pages() )
				++p;
			else
				break;
		gtk_spin_button_set_value( eScoringFacCurrentPage,
					   (__cur_page_app = __cur_page = p)+1);
	}





	void
	bScoreGotoPrevArtifact_clicked_cb()
	// could be emended to work when !pagesize_is_right()
	{
		if ( not pagesize_is_right() || !(__cur_page > 0) )
			return;
		size_t p = __cur_page - 1;
		bool p_has_af;
		while ( !(p_has_af = page_has_artifacts( p)) )
			if ( p > 0 )
				--p;
			else
				break;
		if ( p == 0 && !p_has_af )
			;
		else
			gtk_spin_button_set_value( eScoringFacCurrentPage,
						   (__cur_page_app = __cur_page = p)+1);
	}

	void
	bScoreGotoNextArtifact_clicked_cb()
	{
		if ( pagesize_is_right() || !(__cur_page < SF->total_pages()) )
			return;
		size_t p = __cur_page + 1;
		bool p_has_af;
		while ( !(p_has_af = page_has_artifacts( p)) )
			if ( p < SF->total_pages()-1 )
				++p;
			else
				break;
		if ( p == SF->total_pages()-1 && !p_has_af )
			;
		else
			gtk_spin_button_set_value( eScoringFacCurrentPage,
						   (__cur_page_app = __cur_page = p)+1);
	}








	void
	bScoringFacDrawPower_toggled_cb()
	{
		SF->draw_power = !SF->draw_power;
		for ( auto H = SF->channels.begin(); H != SF->channels.end(); ++H )
			if ( H->have_power() ) {
				g_object_set( G_OBJECT (H->da_power),
					      "visible", SF->draw_power ? TRUE : FALSE,
					      NULL);
				g_object_set( G_OBJECT (H->da_spectrum),
					      "visible", SF->draw_power ? TRUE : FALSE,
					      NULL);
			}
		SF->queue_redraw_all();
	}

	void
	bScoringFacDrawCrosshair_toggled_cb()
	{
		SF->draw_crosshair = !SF->draw_crosshair;
		SF->queue_redraw_all();
	}





	void
	bScoringFacShowFindDialog_toggled_cb( GtkToggleButton *togglebutton,
					      gpointer         user_data)
	{
		if ( gtk_toggle_button_get_active( togglebutton) ) {
			gtk_widget_show_all( (GtkWidget*)wPattern);
		} else
			gtk_widget_hide( (GtkWidget*)wPattern);
	}



	void
	bScoringFacShowPhaseDiffDialog_toggled_cb( GtkToggleButton *togglebutton,
						   gpointer         user_data)
	{
		if ( gtk_toggle_button_get_active( togglebutton) ) {
			gtk_widget_show_all( (GtkWidget*)wPhaseDiff);
		} else
			gtk_widget_hide( (GtkWidget*)wPhaseDiff);
	}







// -- PageSelection


	void
	iSFPageSelectionMarkArtifact_activate_cb( GtkMenuItem *menuitem, gpointer user_data)
	{
		if ( marquee_to_az() > 0 )
			__clicked_channel->mark_region_as_artifact( __pattern_ia, __pattern_iz, true);
	}

	void
	iSFPageSelectionClearArtifacts_activate_cb( GtkMenuItem *menuitem, gpointer user_data)
	{
		if ( marquee_to_az() > 0 )
			__clicked_channel->mark_region_as_artifact( __pattern_ia, __pattern_iz, false);
	}


	// void
	// iSFPageSelectionInspectOne_activate_cb( GtkMenuItem *menuitem, gpointer user_data)
	// {
	// 	FAFA;
	// }

	// void
	// iSFPageSelectionInspectMany_activate_cb( GtkMenuItem *menuitem,
	// 					 gpointer     user_data)
	// {
	// 	FAFA;
	// }










	void
	bSFAccept_clicked_cb( GtkButton *button, gpointer user_data)
	{
		gtk_widget_hide( (GtkWidget*)wPattern);
		gtk_widget_hide( (GtkWidget*)wScoringFacility);
		gtk_widget_queue_draw( (GtkWidget*)cMeasurements);

		delete SF;
	}


	void
	iSFAcceptAndTakeNext_activate_cb( GtkMenuItem *menuitem, gpointer user_data)
	{
		set_cursor_busy( true, (GtkWidget*)wScoringFacility);
		const char
			*j = SF->channels.front().recording.subject(),
			*d = SF->channels.front().recording.session(),
			*e = SF->channels.front().recording.episode();
		agh::CSubject& J = AghCC->subject_by_x(j);
		auto& EE = J.measurements[d].episodes;
		// auto E = find( EE.begin(), EE.end(), e);
		// guaranteed to have next(E)

		delete SF;

		SF = new SScoringFacility( J, d,
					   next( find( EE.begin(), EE.end(), e)) -> name());
		gtk_widget_show_all( (GtkWidget*)wScoringFacility);
		set_cursor_busy( false, (GtkWidget*)wScoringFacility);
	}



// ------- cleanup

	gboolean
	wScoringFacility_delete_event_cb( GtkWidget *widget,
					  GdkEvent  *event,
					  gpointer   user_data)
	{
		delete SF;

		gtk_widget_hide( (GtkWidget*)wPattern);
		gtk_widget_hide( (GtkWidget*)wScoringFacility);
		gtk_widget_queue_draw( (GtkWidget*)cMeasurements);

		return TRUE; // to stop other handlers from being invoked for the event
	}





// -------- colours


	void
	bColourNONE_color_set_cb( GtkColorButton *widget,
				  gpointer        user_data)
	{
		CwB[TColour::score_none].acquire();
	}

	void
	bColourNREM1_color_set_cb( GtkColorButton *widget,
				   gpointer        user_data)
	{
		CwB[TColour::score_nrem1].acquire();
	}


	void
	bColourNREM2_color_set_cb( GtkColorButton *widget,
				   gpointer        user_data)
	{
		CwB[TColour::score_nrem2].acquire();
	}


	void
	bColourNREM3_color_set_cb( GtkColorButton *widget,
				   gpointer        user_data)
	{
		CwB[TColour::score_nrem3].acquire();
	}


	void
	bColourNREM4_color_set_cb( GtkColorButton *widget,
				   gpointer        user_data)
	{
		CwB[TColour::score_nrem4].acquire();
	}

	void
	bColourREM_color_set_cb( GtkColorButton *widget,
				 gpointer        user_data)
	{
		CwB[TColour::score_rem].acquire();
	}

	void
	bColourWake_color_set_cb( GtkColorButton *widget,
				  gpointer        user_data)
	{
		CwB[TColour::score_wake].acquire();
	}



	void
	bColourPowerSF_color_set_cb( GtkColorButton *widget,
				     gpointer        user_data)
	{
		CwB[TColour::power_sf].acquire();
	}


	void
	bColourHypnogram_color_set_cb( GtkColorButton *widget,
				       gpointer        user_data)
	{
		CwB[TColour::hypnogram].acquire();
	}

	void
	bColourArtifacts_color_set_cb( GtkColorButton *widget,
				       gpointer        user_data)
	{
		CwB[TColour::artifact].acquire();
	}



	void
	bColourTicksSF_color_set_cb( GtkColorButton *widget,
				     gpointer        user_data)
	{
		CwB[TColour::ticks_sf].acquire();
	}

	void
	bColourLabelsSF_color_set_cb( GtkColorButton *widget,
				      gpointer        user_data)
	{
		CwB[TColour::labels_sf].acquire();
	}

	void
	bColourCursor_color_set_cb( GtkColorButton *widget,
				    gpointer        user_data)
	{
		CwB[TColour::cursor].acquire();
	}


	void
	bColourBandDelta_color_set_cb( GtkColorButton *widget,
				       gpointer        user_data)
	{
		CwB[TColour::band_delta].acquire();
	}
	void
	bColourBandTheta_color_set_cb( GtkColorButton *widget,
				       gpointer        user_data)
	{
		CwB[TColour::band_theta].acquire();
	}
	void
	bColourBandAlpha_color_set_cb( GtkColorButton *widget,
				       gpointer        user_data)
	{
		CwB[TColour::band_alpha].acquire();
	}
	void
	bColourBandBeta_color_set_cb( GtkColorButton *widget,
				      gpointer        user_data)
	{
		CwB[TColour::band_beta].acquire();
	}
	void
	bColourBandGamma_color_set_cb( GtkColorButton *widget,
				       gpointer        user_data)
	{
		CwB[TColour::band_gamma].acquire();
	}

} // extern "C"


} // namespace aghui


// EOF

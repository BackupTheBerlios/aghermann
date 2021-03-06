// ;-*-C++-*-
/*
 *       File name:  ui/modelrun-facility.cc
 *         Project:  Aghermann
 *          Author:  Andrei Zavada <johnhommer@gmail.com>
 * Initial version:  2008-07-01
 *
 *         Purpose:  modelrun facility
 *
 *         License:  GPL
 */


#include "../libagh/model.hh"
#include "misc.hh"
#include "ui.hh"
#include "expdesign.hh"
#include "modelrun-facility.hh"

#if HAVE_CONFIG_H
#  include <config.h>
#endif


using namespace std;
using namespace aghui;

inline namespace {
	unsigned short __score_hypn_depth[8] = {
		0, 20, 23, 30, 33, 5, 10, 1
	};
}


aghui::SModelrunFacility::SModelrunFacility( agh::CSimulation& csim, SExpDesignUI& parent)
  : csimulation (csim),
// subject is known only by name, so look up his full object now
    csubject (parent.ED->subject_by_x( csim.subject())),
    // not sure we need this though
    display_factor (1.),
    zoomed_episode (-1),
    SWA_smoothover (0),
    _p (parent)
{
	builder = gtk_builder_new();
	if ( !gtk_builder_add_from_file( builder, PACKAGE_DATADIR "/" PACKAGE "/ui/agh-ui-mf.glade", NULL) ) {
		g_object_unref( (GObject*)builder);
		throw runtime_error( "SModelrunFacility::SModelrunFacility(): Failed to load GtkBuilder object");
	}
	_suppress_Vx_value_changed = true;
	if ( construct_widgets() )
		throw runtime_error( "SModelrunFacility::SModelrunFacility(): Failed to construct own widgets");

      // do a single cycle to produce SWA_sim and Process S
	cf = csim.snapshot();

      // determine SWA_max, for scaling purposes;
	SWA_max = 0.;
	for ( size_t p = 0; p < csim.timeline().size(); ++p )
		if ( csim[p].SWA > SWA_max )
			SWA_max = csim[p].SWA;

      // // also smooth the SWA course
      // 	if ( SWA_smoothover ) {
      // 		for ( size_t p = 0; p < __timeline_pages; ++p )
      // 			if ( p < __smooth_SWA_course || p >= __timeline_pages-1 - __smooth_SWA_course )
      // 				tmp[p] = __SWA_course[p];
      // 			else {
      // 				double sum = 0.;
      // 				for ( size_t q = p - __smooth_SWA_course; q <= p + __smooth_SWA_course; ++q )
      // 					sum += __SWA_course[q];
      // 				tmp[p] = sum / (2 * __smooth_SWA_course + 1);
      // 			}
      // 		memcpy( __SWA_course, tmp, __timeline_pages * sizeof(double));
      // 	}

	snprintf_buf( "Simulation: %s (%s) in %s, %g-%g Hz",
		      csim.subject(),
		      _p.AghD(), _p.AghH(), csim.freq_from(), csim.freq_upto());
	gtk_window_set_title( wModelrunFacility,
			      __buf__);
	gtk_window_set_default_size( wModelrunFacility,
				     gdk_screen_get_width( gdk_screen_get_default()) * .80,
				     gdk_screen_get_height( gdk_screen_get_default()) * .46);

	update_infobar();
	_suppress_Vx_value_changed = false;

	snprintf_buf( "sim start at p. %zu, end at p. %zu, baseline end at p. %zu,\n"
		      "%zu pp with SWA, %zu pp in bed;\n"
		      "SWA_L = %g, SWA_0 = %g, SWA_100 = %g\n",
		      csim.sim_start(), csim.sim_end(), csim.baseline_end(),
		      csim.pages_with_swa(), csim.pages_in_bed(),
		      csim.SWA_L(), csim.SWA_0(), csim.SWA_100());
	gtk_text_buffer_set_text( log_text_buffer, __buf__, -1);

	gtk_widget_show_all( (GtkWidget*)wModelrunFacility);
}


aghui::SModelrunFacility::~SModelrunFacility()
{
	gtk_widget_destroy( (GtkWidget*)wModelrunFacility);
	g_object_unref( (GObject*)builder);
}


void
aghui::SModelrunFacility::siman_param_printer( void *xp)
{
//	memcpy( __t_set.tunables, xp, __t_set.n_tunables * sizeof(double));
	// access this directly, no?
	gtk_widget_queue_draw( (GtkWidget*)daMFProfile);
	update_infobar();
	while ( gtk_events_pending() )
		gtk_main_iteration();
}


SModelrunFacility*
	aghui::__MF;

void
aghui::SModelrunFacility::MF_siman_param_printer( void *xp)
{
	__MF -> siman_param_printer( xp);
}





void
aghui::SModelrunFacility::draw_timeline( cairo_t *cr)
{
      // empirical SWA
	size_t	cur_ep;

	if ( zoomed_episode != -1 ) {
		size_t	ep_start = csimulation.nth_episode_start_page( zoomed_episode),
			ep_end   = csimulation.nth_episode_end_page  ( zoomed_episode);
		draw_episode( cr,
			      zoomed_episode,
			      ep_start, ep_end,
			      ep_start, ep_end);
		draw_ticks( cr, ep_start, ep_end);
	} else {
	      // draw day and night
		{
			time_t	timeline_start = csimulation.mm_list().front()->start(),
				timeline_end   = csimulation.mm_list().back()->end();

			cairo_pattern_t *cp = cairo_pattern_create_linear( 0., 0., da_wd, 0);
			struct tm clock_time;
			memcpy( &clock_time, localtime( &timeline_start), sizeof(clock_time));
			clock_time.tm_hour = 4;
			clock_time.tm_min = clock_time.tm_sec = 0;
			time_t	dawn = mktime( &clock_time),
				t;
			bool up = true;

			for ( t = dawn; t < timeline_end; t += 3600 * 12, up = !up )
				if ( t > timeline_start )
					cairo_pattern_add_color_stop_rgba( cp,
									   (difftime( t, timeline_start)/(timeline_end-timeline_start)),
									   up?.7:.8, up?.6:.8, up?1.:.8, .5);
			cairo_set_source( cr, cp);
			cairo_rectangle( cr, 0., 0., da_wd, da_ht);
			cairo_fill( cr);
			cairo_stroke( cr);
			cairo_pattern_destroy( cp);
		}
	      // draw episodes

		for ( cur_ep = 0; cur_ep < csimulation.mm_list().size(); ++cur_ep )
			draw_episode( cr,
				      cur_ep,
				      csimulation.nth_episode_start_page( cur_ep),
				      csimulation.nth_episode_end_page  ( cur_ep),
				      0, csimulation.timeline().size());
	      // Process S in one go for the entire timeline
		cairo_set_line_width( cr, 2.);
		_p.CwB[SExpDesignUI::TColour::process_s].set_source_rgba( cr);
		cairo_move_to( cr, tl_pad + 0,
			       da_ht - lgd_margin-hypn_depth
			       - csimulation[0].S * da_ht / SWA_max * display_factor);
		for ( size_t i = 1; i < csimulation.timeline().size(); ++i )
			cairo_line_to( cr,
				       tl_pad + (float)i / csimulation.timeline().size() * da_wd_actual(),
				       da_ht - lgd_margin-hypn_depth
				       - csimulation[i].S * da_ht / SWA_max * display_factor);
		cairo_stroke( cr);

		draw_ticks( cr, 0, csimulation.timeline().size());
	}


      // zeroline
	cairo_set_line_width( cr, .3);
	cairo_set_source_rgb( cr, 0, 0, 0);
	cairo_move_to( cr, 0., da_ht-lgd_margin-hypn_depth + 5);
	cairo_rel_line_to( cr, da_wd, 0.);

	cairo_stroke( cr);
}





void
aghui::SModelrunFacility::draw_episode( cairo_t *cr,
					size_t ep,
					size_t ep_start, size_t ep_end,
					size_t tl_start, size_t tl_end)
{
	size_t i;

	if ( zoomed_episode != -1 ) {
		_p.CwB[SExpDesignUI::TColour::paper_mr].set_source_rgb( cr);
		cairo_rectangle( cr, 0., 0., da_wd, da_ht);
		cairo_fill( cr);
		cairo_stroke( cr);
	}

	cairo_set_line_width( cr, .5);
	_p.CwB[SExpDesignUI::TColour::swa].set_source_rgba( cr, 1.);

	size_t tl_len = tl_end - tl_start;
	cairo_move_to( cr, tl_pad + (float)(ep_start - tl_start) / tl_len * da_wd_actual(),
		       da_ht - lgd_margin-hypn_depth
		       - csimulation[ep_start].SWA / SWA_max * (float)da_ht * display_factor);
	for ( i = 1; i < ep_end - ep_start; ++i )
		cairo_line_to( cr,
			       tl_pad + (float)(ep_start - tl_start + i) / tl_len * da_wd_actual(),
			       da_ht - lgd_margin-hypn_depth
			       - csimulation[ep_start + i].SWA * (float)da_ht / SWA_max * display_factor);

	cairo_stroke( cr);

	cairo_set_source_rgba( cr, 0., 0., 0., .6);
	cairo_set_font_size( cr, (zoomed_episode == -1 ) ? 9 : 14);
	cairo_select_font_face( cr, "serif", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
	cairo_move_to( cr, tl_pad + (float)(ep_start - tl_start)/tl_len * da_wd_actual(), 16);
	cairo_show_text( cr, csimulation.mm_list()[ep]->source().episode.c_str());
	cairo_stroke( cr);

      // simulated SWA
	cairo_set_line_width( cr, 2);
	_p.CwB[SExpDesignUI::TColour::swa_sim].set_source_rgba( cr);
	cairo_move_to( cr, tl_pad + (float)(ep_start - tl_start) / tl_len * da_wd_actual(),
		       da_ht - lgd_margin-hypn_depth
		       - csimulation[ep_start].SWA_sim * da_ht / SWA_max * display_factor);
	for ( i = 1; i < ep_end - ep_start; ++i )
		cairo_line_to( cr,
			       tl_pad + (float)(ep_start - tl_start + i) / tl_len * da_wd_actual(),
			       da_ht - lgd_margin-hypn_depth
			       - csimulation[ep_start + i].SWA_sim * da_ht / SWA_max * display_factor);
	cairo_stroke( cr);

      // Process S
	// draw only for zoomed episode: else it is drawn for all in one go
	if ( zoomed_episode != -1 ) {
		cairo_set_line_width( cr, 2.);
		_p.CwB[SExpDesignUI::TColour::process_s].set_source_rgba( cr);
		cairo_move_to( cr, tl_pad + (float)(ep_start - tl_start) / tl_len * da_wd_actual(),
			       da_ht - lgd_margin-hypn_depth
			       - csimulation[ep_start].S * da_ht / SWA_max * display_factor);
		size_t possible_end = ep_end - ep_start +
			((zoomed_episode == (int)csimulation.mm_list().size() - 1) ? 0 : ((float)csimulation.timeline().size()/da_wd_actual() * tl_pad));
		for ( i = 1; i < possible_end; ++i )
			cairo_line_to( cr,
				       tl_pad + (float)(ep_start - tl_start + i) / tl_len * da_wd_actual(),
				       da_ht - lgd_margin-hypn_depth
				       - csimulation[ep_start + i].S * da_ht / SWA_max * display_factor);
		cairo_stroke( cr);
	}

      // hypnogram
	cairo_set_source_rgba( cr, 0., 0., 0., .4);
	cairo_set_line_width( cr, 3.);
	for ( i = 0; i < ep_end - ep_start; ++i ) {
		auto sco = csimulation[i].score();
		if ( sco != agh::SPage::TScore::none ) {
			int y = __score_hypn_depth[ (agh::SPage::TScore_underlying_type)sco ];
			cairo_move_to( cr, tl_pad + (float)(ep_start - tl_start + i  ) / tl_len * da_wd_actual(),
				       da_ht - hypn_depth + y);
			cairo_rel_line_to( cr, 1. / tl_len * da_wd_actual(), 0);
			cairo_stroke( cr);
		}
	}
}


void
aghui::SModelrunFacility::draw_ticks( cairo_t *cr,
				      size_t start, size_t end)
{
      // ticks
	guint	pph = 3600/csimulation.pagesize(),
		pps = pph/2;
	float	tick_spc_rough = (float)(end-start)/(da_wd/120.) / pph,
		tick_spc;
	float	sizes[] = { NAN, .25, .5, 1, 2, 3, 4, 6, 12 };
	size_t i = 8;
	while ( i > 0 && (tick_spc = sizes[i]) > tick_spc_rough )
		--i;
	tick_spc *= pph;

	cairo_set_font_size( cr, 9);
	cairo_select_font_face( cr, "sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
	start = start/pps * pps;  // align to 30 min
	for ( i = start; i < end; i += (unsigned)tick_spc ) {
		_p.CwB[SExpDesignUI::TColour::ticks_mr].set_source_rgba( cr, .4);
		cairo_set_line_width( cr, (i % (24*pph) == 0) ? 1 : .3);
		cairo_move_to( cr, (float)(i-start)/(end-start) * da_wd_actual(), 0);
		cairo_rel_line_to( cr, 0., da_ht);
		cairo_stroke( cr);

		_p.CwB[SExpDesignUI::TColour::labels_mr].set_source_rgba( cr);
		cairo_move_to( cr,
			       (float)(i-start)/(end-start) * da_wd_actual() + 2,
			       da_ht - hypn_depth-lgd_margin + 14);
		snprintf_buf_ts_h( (double)i/pph);
		cairo_show_text( cr, __buf__);
		cairo_stroke( cr);
	}
}








void
aghui::SModelrunFacility::update_infobar()
{
	for_each( eMFVx.begin(), eMFVx.end(),
		  [&] ( pair<GtkSpinButton* const, agh::TTunable>& tuple)
		  {
			  auto t = min((size_t)tuple.second, (size_t)agh::TTunable::_basic_tunables - 1);
			  gtk_spin_button_set_value(
				  tuple.first,
				  csimulation.cur_tset[tuple.second]
				  * agh::STunableSet::stock[t].display_scale_factor);
		  });
	snprintf_buf( "CF = <b>%g</b>\n", cf);
	gtk_label_set_markup( lMFCostFunction, __buf__);
}



// eof

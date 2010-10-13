// ;-*-C-*- *  Time-stamp: "2010-10-12 19:55:13 hmmr"
/*
 *       File name:  ui/measurements.c
 *         Project:  Aghermann
 *          Author:  Andrei Zavada (johnhommer@gmail.com)
 * Initial version:  2008-07-01
 *
 *         Purpose:  
 *
 *         License:  GPL
 */


#include <assert.h>
#include <math.h>
#include <string.h>
#include <time.h>
#include <glade/glade.h>
#include "../core/iface.h"
#include "../core/iface-glib.h"
#include "ui.h"


GtkWidget
	*wEDFFileDetails;

GtkWidget
	*eMsmtChannel,
	*eMsmtSession,
	*cMeasurements,
	*lEDFFileDetailsReport,
	*eMsmtPSDFreqFrom,
	*eMsmtPSDFreqWidth,
	*bMsmtDetails;

static GtkTextBuffer
	*textbuf2;


static GdkGC
	*__gc_power,
	*__gc_ticks,
	*__gc_labels,
	*__gc_hours;
static PangoLayout
	*__pango_layout;
static GString
	*__j_label;

#define RGB_BG        "#838B83"
#define RGB_FG_POWER  "#FF0000"
#define RGB_FG_TICKS  "#228B22"
#define RGB_FG_LABELS "#ADFF2F"
#define RGB_FG_HOURS  "#7FFF00"
static GdkColor
	__bg,
	__fg_power,
	__fg_ticks,
	__fg_labels,
	__fg_hours;



void eMsmtSession_changed_cb(void);
void eMsmtChannel_changed_cb(void);

gint
agh_ui_construct_Measurements( GladeXML *xml)
{
	GtkCellRenderer *renderer;

     // ------------- cMeasurements
	if ( !(cMeasurements = glade_xml_get_widget( xml, "cMeasurements")) )
		return -1;


     // ------------- eMsmtSession
	if ( !(eMsmtSession = glade_xml_get_widget( xml, "eMsmtSession")) )
		return -1;

	gtk_combo_box_set_model( GTK_COMBO_BOX (eMsmtSession),
				 GTK_TREE_MODEL (agh_mSessions));
	g_signal_connect( eMsmtSession, "changed", eMsmtSession_changed_cb, NULL);
	renderer = gtk_cell_renderer_text_new();
	gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (eMsmtSession), renderer, FALSE);
	gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (eMsmtSession), renderer,
					"text", 0,
					NULL);

     // ------------- eMsmtChannel
	if ( !(eMsmtChannel = glade_xml_get_widget( xml, "eMsmtChannel")) )
		return -1;

	gtk_combo_box_set_model( GTK_COMBO_BOX (eMsmtChannel),
				 GTK_TREE_MODEL (agh_mEEGChannels));
	g_signal_connect( eMsmtChannel, "changed", eMsmtChannel_changed_cb, NULL);

	renderer = gtk_cell_renderer_text_new();
	gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (eMsmtChannel), renderer, FALSE);
	gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (eMsmtChannel), renderer,
					"text", 0,
					NULL);

     // ------------- bMsmtXxx
	if ( !(bMsmtDetails  = glade_xml_get_widget( xml, "bMsmtDetails")) )
		return -1;

     // ------------- eMsmtPSDFreq
	if ( !(eMsmtPSDFreqFrom = glade_xml_get_widget( xml, "eMsmtPSDFreqFrom")) ||
	     !(eMsmtPSDFreqWidth = glade_xml_get_widget( xml, "eMsmtPSDFreqWidth")) )
		return -1;

     // ------------- wEDFFileDetails
	if ( !(wEDFFileDetails = glade_xml_get_widget( xml, "wEDFFileDetails")) ||
	     !(lEDFFileDetailsReport = glade_xml_get_widget( xml, "lEDFFileDetailsReport")) )
		return -1;

	g_object_set( lEDFFileDetailsReport,
		      "tabs", pango_tab_array_new_with_positions( 2, TRUE,
								  PANGO_TAB_LEFT, 130,
								  PANGO_TAB_LEFT, 190),
		      NULL);
	textbuf2 = gtk_text_view_get_buffer( GTK_TEXT_VIEW (lEDFFileDetailsReport));


      // --- assorted static objects
	GdkColormap* cmap = gtk_widget_get_colormap( cMeasurements);
	GdkGCValues __gc_values;
	gdk_color_parse( RGB_BG, &__bg),
		gdk_colormap_alloc_color( cmap, &__bg, FALSE, TRUE);
	gdk_color_parse( RGB_FG_POWER, &__fg_power),
		gdk_colormap_alloc_color( cmap, &__fg_power, FALSE, TRUE);
	gdk_color_parse( RGB_FG_TICKS, &__fg_ticks),
		gdk_colormap_alloc_color( cmap, &__fg_ticks, FALSE, TRUE);
	gdk_color_parse( RGB_FG_LABELS, &__fg_labels),
		gdk_colormap_alloc_color( cmap, &__fg_labels, FALSE, TRUE);
	gdk_color_parse( RGB_FG_HOURS, &__fg_hours),
		gdk_colormap_alloc_color( cmap, &__fg_hours, FALSE, TRUE);

	__gc_values.line_width = 1;
	__gc_values.line_style = GDK_LINE_SOLID;
	__gc_values.join_style = GDK_JOIN_ROUND;
	__gc_values.cap_style = GDK_CAP_BUTT;
	__gc_values.background = __bg;
	__gc_values.foreground = __fg_power;
	__gc_power = gtk_gc_get( agh_visual->depth, cmap,
				 &__gc_values, GDK_GC_FOREGROUND | GDK_GC_BACKGROUND | GDK_GC_LINE_WIDTH | GDK_GC_LINE_STYLE);

	__gc_values.line_width = 1;
	__gc_values.foreground = __fg_ticks;
	__gc_ticks = gtk_gc_get( agh_visual->depth, cmap,
				 &__gc_values, GDK_GC_FOREGROUND);

	__gc_values.foreground = __fg_labels;
	__gc_labels = gtk_gc_get( agh_visual->depth, cmap,
				  &__gc_values, GDK_GC_FOREGROUND);

	__gc_values.foreground = __fg_hours;
	__gc_hours = gtk_gc_get( agh_visual->depth, cmap,
				 &__gc_values, GDK_GC_FOREGROUND);


	__pango_layout = gtk_widget_create_pango_layout( cMeasurements, "");

	__j_label = g_string_sized_new( 200);

	return 0;
}


void
agh_ui_destruct_Measurements()
{
	gtk_gc_release(__gc_power);
	gtk_gc_release(__gc_ticks);
	gtk_gc_release(__gc_labels);
	gtk_gc_release(__gc_hours);
	g_string_free( __j_label, TRUE);
}




void
eMsmtSession_changed_cb()
{
	gint oldval = AghDi;
	AghDi = gtk_combo_box_get_active( GTK_COMBO_BOX (eMsmtSession));

	if ( oldval != AghDi ) {
		gtk_combo_box_set_active( GTK_COMBO_BOX (eSimulationsSession), AghDi);
		agh_populate_cMeasurements();
	}
//	gtk_widget_queue_draw( cMeasurements);
}

void
eMsmtChannel_changed_cb()
{
	gint oldval = AghTi;
	AghTi = gtk_combo_box_get_active( GTK_COMBO_BOX (eMsmtChannel));

	if ( oldval != AghTi ) {
		gtk_combo_box_set_active( GTK_COMBO_BOX (eSimulationsChannel), AghTi);
		agh_populate_cMeasurements();
	}
//	gtk_widget_queue_draw( cMeasurements);
}




void
eMsmtPSDFreqFrom_value_changed_cb()
{
	AghQuickViewFreqFrom = gtk_spin_button_get_value( GTK_SPIN_BUTTON (eMsmtPSDFreqFrom));
	AghQuickViewFreqUpto = AghQuickViewFreqFrom + gtk_spin_button_get_value( GTK_SPIN_BUTTON (eMsmtPSDFreqWidth));
	agh_populate_cMeasurements();
}

void
eMsmtPSDFreqWidth_value_changed_cb()
{
	AghQuickViewFreqUpto = AghQuickViewFreqFrom + gtk_spin_button_get_value( GTK_SPIN_BUTTON (eMsmtPSDFreqWidth));
	agh_populate_cMeasurements();
}

void
cMsmtPSDFreq_map_cb()
{
	gtk_spin_button_set_value( GTK_SPIN_BUTTON (eMsmtPSDFreqWidth), AghQuickViewFreqUpto - AghQuickViewFreqFrom);
	gtk_spin_button_set_value( GTK_SPIN_BUTTON (eMsmtPSDFreqFrom), AghQuickViewFreqFrom);
	g_signal_connect( eMsmtPSDFreqFrom, "value-changed", G_CALLBACK (eMsmtPSDFreqFrom_value_changed_cb), NULL);
	g_signal_connect( eMsmtPSDFreqWidth, "value-changed", G_CALLBACK (eMsmtPSDFreqWidth_value_changed_cb), NULL);
}







guint	AghTimelinePPH = 20;
float	AghQuickViewFreqFrom = 2.,
	AghQuickViewFreqUpto = 3.;
gboolean
	AghMsmtViewDrawPowerSolid = FALSE,
	AghMsmtViewDrawDetails = TRUE;

static time_t
	AghTimelineStart,
	AghTimelineEnd;

static guint
	__tl_left_margin = 60,
	__tl_right_margin = 20,
	__timeline_length;

static inline guint
T2P( time_t t)
{
	return difftime( t, AghTimelineStart) / 3600 * AghTimelinePPH;
}

static inline time_t
P2T( guint p)
{
	return (double)p * 3600 / AghTimelinePPH + AghTimelineStart;
}



typedef struct {
	struct SSubject
		subject;
	GtkWidget
		*da;
	GArray	*power;
} SSubjectPresentation;

typedef struct {
	const char
		*name;
	GArray	*subjects;
	gboolean
		visible;
	GtkWidget
		*expander,
		*vbox;
} SGroupPresentation;

static void
free_group_presentation( SGroupPresentation* g)
{
	for ( guint j = 0; j < g->subjects->len; ++j ) {
		SSubjectPresentation *J = &Ai (g->subjects, SSubjectPresentation, j);
		agh_SSubject_destruct( &J->subject);
		g_array_free( J->power, TRUE);
		// gtk_widget_destroy( J->da);  // this is done in gtk_container_foreach( cMeasurements, gtk_widget_destroy)
	}
	g_array_free( g->subjects, TRUE);
}

static GArray	*GG;








#define JTLDA_HEIGHT 60

gboolean xExpander_expose_event_cb( GtkWidget* un, GdkEventExpose* us, gpointer ed)
{
	return TRUE;
}

gboolean daSubjectTimeline_button_press_event_cb( GtkWidget*, GdkEventButton*, gpointer);
gboolean daSubjectTimeline_scroll_event_cb( GtkWidget*, GdkEventScroll*, gpointer);
gboolean daSubjectTimeline_expose_event_cb( GtkWidget*, GdkEventExpose*, gpointer);




static SSubjectPresentation *J_paintable;

void
__paint_one_subject_episodes()
{
	struct SSubject* _j = &J_paintable->subject;

	time_t	j_timeline_start = _j->sessions[AghDi].episodes[0].start_rel;

	for ( guint e = 0; e < _j->sessions[AghDi].n_episodes; ++e ) {
		// ...( J->subject->sessions[AghDi].episodes[e].; // rather use agh_msmt_find_by_jdeh than look up the matching channel
		TRecRef recref = agh_msmt_find_by_jdeh( _j->name, AghD, _j->sessions[AghDi].episodes[e].name, AghT);
		if ( recref == NULL ) { // this can happen, too
			continue;
		}
		GArray *episode_signal = g_array_new( FALSE, FALSE, sizeof(double));
		agh_msmt_get_power_course_in_range_as_double_garray( recref,
								     AghQuickViewFreqFrom, AghQuickViewFreqUpto,
								     episode_signal);

		time_t	j_e_start = _j->sessions[AghDi].episodes[e].start_rel;
		//j_e_end = _j->sessions[AghDi].episodes[e].end_rel;
		// put power on the global timeline

		guint	pagesize = agh_msmt_get_pagesize( _j->sessions[AghDi].episodes[0].recordings[0]);

		memcpy( &Ai (J_paintable->power, double, (j_e_start - j_timeline_start) / pagesize),
			&Ai (episode_signal, double, 0),
			episode_signal->len * sizeof(double));

		g_array_free( episode_signal, TRUE);
	}
}

void
agh_populate_cMeasurements()
{
      // prepare structures for the first run
	if ( !GG )
		GG = g_array_new( FALSE, FALSE, sizeof(SGroupPresentation));

	gtk_container_foreach( GTK_CONTAINER (cMeasurements),
			       (GtkCallback) gtk_widget_destroy,
			       NULL);
	for ( guint g = 0; g < GG->len; ++g )
		free_group_presentation( &Ai (GG, SGroupPresentation, g));
	g_array_set_size( GG, AghGs);

	time_t	earliest_start = (time_t)-1,
		latest_end = (time_t)-1;

      // first pass: determine common timeline and collect episodes' power
	guint g = 0;
	while ( AghGG[g] ) {  // a faster agh_group_find_first()
		SGroupPresentation* G = &Ai (GG, SGroupPresentation, g);
		G->name = AghGG[g];
		G->subjects = g_array_new( FALSE, FALSE, sizeof(SSubjectPresentation));
		g_array_set_size( G->subjects,
				  agh_subject_get_n_of_in_group( AghGG[g]));
		guint j = 0;
		agh_subject_find_first_in_group( AghGG[g],
						 &Ai (G->subjects, SSubjectPresentation, j).subject);
		while ( j < G->subjects->len ) {
			SSubjectPresentation* J = &Ai (G->subjects, SSubjectPresentation, j);
			struct SSubject* _j = &J->subject;

			guint	j_n_episodes = _j->sessions[AghDi].n_episodes;
			time_t	j_timeline_start = _j->sessions[AghDi].episodes[0].start_rel,
				j_timeline_end   = _j->sessions[AghDi].episodes[ j_n_episodes-1 ].end_rel;

		      // determine timeline global start and end
			if ( earliest_start == (time_t)-1) {
				earliest_start = j_timeline_start;
				latest_end = j_timeline_end;
			}
			if ( earliest_start > j_timeline_start )
				earliest_start = j_timeline_start;
			if ( latest_end < j_timeline_end )
				latest_end = j_timeline_end;

		      // allocate subject timeline
			guint	pagesize = agh_msmt_get_pagesize( _j->sessions[AghDi].episodes[0].recordings[0]),
				total_pages = (j_timeline_end - j_timeline_start) / pagesize;

			J->power = g_array_sized_new( FALSE, TRUE, sizeof(double), total_pages);
			g_array_set_size( J->power, total_pages);

			J_paintable = J;
			__paint_one_subject_episodes();

			++j;  // the following will pass a pointer past-allocated area, but nothing will be written there
			agh_subject_find_next_in_group( &Ai (G->subjects, SSubjectPresentation, j).subject);
		}

		++g;
	}

	AghTimelineStart = earliest_start;
	AghTimelineEnd   = latest_end;
	__timeline_length = (double)(AghTimelineEnd - AghTimelineStart) / 3600 * AghTimelinePPH;

//	fprintf( stderr, "agh_populate_cMeasurements(): common timeline:\n");
//	fputs (asctime (localtime(&earliest_start)), stderr);
//	fputs (asctime (localtime(&latest_end)), stderr);

      // walk again, set timeline drawing area length
	g = 0;
	while ( AghGG[g] ) {
		SGroupPresentation* G = &Ai (GG, SGroupPresentation, g);
		G->expander = gtk_expander_new( AghGG[g]);
		g_object_set( G_OBJECT (G->expander),
			      "visible", TRUE,
			      "expanded", TRUE,
			      "height-request", 0,
//			      "fill", FALSE,
//			      "expand", FALSE,
			      NULL);
//		g_signal_connect_after( G->expander, "expose-event",
//					G_CALLBACK (xExpander_expose_event_cb),
//					(gpointer)&G);
		gtk_box_pack_start( GTK_BOX (cMeasurements),
				    G->expander, TRUE, TRUE, 3);
		gtk_container_add( GTK_CONTAINER (G->expander),
				   G->vbox = gtk_vbox_new( TRUE, 1));
		g_object_set( G_OBJECT (G->vbox),
//			      "fill", FALSE,
//			      "expand", FALSE,
			      "height-request", -1,
			      NULL);
		guint j = 0;
		while ( j < G->subjects->len ) {
			SSubjectPresentation* J = &Ai (G->subjects, SSubjectPresentation, j);
			gtk_box_pack_start( GTK_BOX (G->vbox),
					    J->da = gtk_drawing_area_new(), TRUE, TRUE, 2);

			g_signal_connect_after( J->da, "expose-event",
						G_CALLBACK (daSubjectTimeline_expose_event_cb),
						(gpointer)J);
			g_signal_connect_after( J->da, "scroll-event",
						G_CALLBACK (daSubjectTimeline_scroll_event_cb),
						(gpointer)J);
			g_signal_connect_after( J->da, "button-press-event",
						G_CALLBACK (daSubjectTimeline_button_press_event_cb),
						(gpointer)J);
			g_object_set( G_OBJECT (J->da),
				      "app-paintable", TRUE,
				      "double-buffered", TRUE,
//				      "fill", TRUE,
//				      "expand", FALSE,
				      "height-request", JTLDA_HEIGHT,
				      "width-request", __timeline_length + __tl_left_margin + __tl_right_margin,
				      NULL);

			gtk_widget_add_events( J->da,
					       (GdkEventMask) GDK_KEY_PRESS_MASK | GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK);

			gtk_widget_modify_fg( J->da, GTK_STATE_NORMAL, &__fg_power);
			gtk_widget_modify_bg( J->da, GTK_STATE_NORMAL, &__bg);

			++j;
		}
		++g;
	}
	gtk_widget_show_all( cMeasurements);
}






#define M_PI 3.14159


gboolean
daSubjectTimeline_expose_event_cb( GtkWidget *container, GdkEventExpose *event, gpointer userdata)
{
	static GdkGC *circadian_gc = NULL;
	static GArray *lines = NULL;
	if ( !lines ) {
		lines = g_array_new( FALSE, FALSE, sizeof(GdkPoint));
		circadian_gc = gdk_gc_new( cMeasurements->window);
	}

	g_array_set_size( lines, __timeline_length * 2);  // in case AghMsmtViewDrawPowerSolid is TRUE

	GdkColor circadian_color = { 0, 65535, 65535, 65535 };

	SSubjectPresentation* J = (SSubjectPresentation*)userdata;

      // draw day and night
	for ( guint c = 0; c < __timeline_length; ++c ) {
		time_t t = P2T(c);
		struct tm clock_time;
		memcpy( &clock_time, localtime(&t), sizeof(clock_time));
		float clock_h = clock_time.tm_hour + (float)clock_time.tm_min/60 + (float)clock_time.tm_sec/3600;

		float day_fraction = (1 + sinf( (clock_h - 5)/ 24 * 2.*M_PI))/2;
		circadian_color.red = circadian_color.green = day_fraction*65535/1.6;
		gdk_gc_set_rgb_fg_color( circadian_gc, &circadian_color);
		gdk_draw_line( J->da->window, circadian_gc,
			       c + __tl_left_margin, 0,
			       c + __tl_left_margin, JTLDA_HEIGHT - 12);
	}
      // draw hours
	struct tm tl_start_fixed_tm;
	memcpy( &tl_start_fixed_tm, localtime( &AghTimelineStart), sizeof(struct tm));
	// determine the latest full hour before AghTimelineStart
	tl_start_fixed_tm.tm_min = 0;
	time_t tl_start_fixed = mktime( &tl_start_fixed_tm);

	for ( time_t t = tl_start_fixed; t < AghTimelineEnd + 3600; t += 3600 ) {
		guint x = T2P(t);
		guint clock_h = localtime(&t)->tm_hour;
		if ( clock_h % 6 == 0 ) {
			gdk_draw_line( J->da->window, __gc_ticks,
				       __tl_left_margin + x, JTLDA_HEIGHT - 12,
				       __tl_left_margin + x, JTLDA_HEIGHT - 10);
			g_string_printf( __j_label, "<small><b>%d</b></small>", clock_h);
			pango_layout_set_markup( __pango_layout, __j_label->str, -1);
			PangoRectangle extents;
			pango_layout_get_pixel_extents( __pango_layout, &extents, NULL);
			gdk_draw_layout( J->da->window, __gc_hours,
					 __tl_left_margin + x - extents.width/2, JTLDA_HEIGHT-13,
					 __pango_layout);
		} else
			gdk_draw_line( J->da->window, __gc_ticks,
				       __tl_left_margin + x, JTLDA_HEIGHT - 12,
				       __tl_left_margin + x, JTLDA_HEIGHT - 8);
	}

      // draw episode
	struct SSubject *_j = &J->subject;

	g_string_printf( __j_label, "<big>%s</big>", _j->name);
	pango_layout_set_markup( __pango_layout, __j_label->str, -1);
	gdk_draw_layout( J->da->window, __gc_labels,
			 3, 5,
			 __pango_layout);

	gsize	j_n_episodes = _j->sessions[AghDi].n_episodes;
	gsize	j_tl_pixel_start = difftime( _j->sessions[AghDi].episodes[0].start_rel, AghTimelineStart) / 3600 * AghTimelinePPH,
		j_tl_pixel_end   = difftime( _j->sessions[AghDi].episodes[j_n_episodes-1].end_rel, AghTimelineStart) / 3600 * AghTimelinePPH,
		j_tl_pixels = j_tl_pixel_end - j_tl_pixel_start;
//			printf( "%s: pixels from %zu to %zu\n", _j->name, j_tl_pixel_start, j_tl_pixel_end);

	// boundaries
	for ( guint e = 0; e < _j->sessions[AghDi].n_episodes; ++e ) {
		struct SEpisode *_e = &_j->sessions[AghDi].episodes[e];
		guint	e_pix_start = T2P( _e->start_rel),
			e_pix_end   = T2P( _e->end_rel);
//				printf( "%s: %d: %d-%d\n", _j->name, e, e_pix_start, e_pix_end);
		gdk_draw_rectangle( J->da->window, __gc_ticks, TRUE,
				    __tl_left_margin + e_pix_start, JTLDA_HEIGHT-3,
				    e_pix_end-e_pix_start, 3);

		if ( AghMsmtViewDrawDetails ) {
			// episode start timestamp
			char date_str[80];
			strftime( date_str, 79, "%F %T",
				  localtime( &_j->sessions[AghDi].episodes[e].start));
			g_string_set_size( __j_label, 80);
			g_string_printf( __j_label, "<small><b>%s\n%s</b></small>",
					 date_str, _j->sessions[AghDi].episodes[e].name);
			pango_layout_set_markup( __pango_layout,
						 __j_label->str,
						 -1);
			gdk_draw_layout( J->da->window, __gc_labels,
					 __tl_left_margin + e_pix_start, 2,
					 __pango_layout);
		}
	}

	// power
#define X(x) Ai (J->power, double, x)
#define L(x) Ai (lines, GdkPoint, x)
	guint i, k, m;
	if ( AghMsmtViewDrawPowerSolid ) {
		for ( i = j_tl_pixel_start + __tl_left_margin, k = 0, m = 0;
		      k < j_tl_pixels; ++i, ++k, m += 2 ) {
			L(m).x = i;
			L(m).y = -X( (gsize)((double)k / j_tl_pixels * J->power->len) )
				* AghPPuV2
				+ JTLDA_HEIGHT-12;
			L(m+1).x = i+1;
			L(m+1).y = AghPPuV2 + JTLDA_HEIGHT-12;
		}
		gdk_draw_lines( J->da->window, __gc_power,
				(GdkPoint*)lines->data, m);
	} else {
		for ( i = j_tl_pixel_start + __tl_left_margin, k = 0;
		      k < j_tl_pixels; ++i, ++k ) {
			L(k).x = i;
			L(k).y = -X( (gsize)((double)k / j_tl_pixels * J->power->len) )
				* AghPPuV2
				+ JTLDA_HEIGHT-12;
		}
		gdk_draw_lines( J->da->window, __gc_power,
				(GdkPoint*)lines->data, k);
	}
#undef L
#undef X

	return FALSE;
}








static gint
get_episode_from_timeline_click( struct SSubject* _j, guint along, const char **which_e)
{
	along -= __tl_left_margin;
	for ( gint e = 0; e < _j->sessions[AghDi].n_episodes; ++e ) {
		struct SEpisode *_e = &_j->sessions[AghDi].episodes[e];
		if ( along >= T2P(_e->start_rel) && along <= T2P(_e->end_rel) ) {
			(*which_e) = _e->name;
			return e;
		}
	}
	return -1;
}

gboolean
daSubjectTimeline_button_press_event_cb( GtkWidget *widget, GdkEventButton *event, gpointer userdata)
{
	static GString *window_title = NULL;
	if ( !window_title )
		window_title = g_string_sized_new( 120);

	SSubjectPresentation *J = (SSubjectPresentation*)userdata;
	struct SSubject *_j = &J->subject;

	gint wd, ht;
	gdk_drawable_get_size( widget->window, &wd, &ht);
	const char *real_episode_name;
	gint clicked_episode = get_episode_from_timeline_click( _j, event->x, &real_episode_name);
	if ( clicked_episode != -1 ) {
		// should some episodes be missing, we make sure the correct one gets identified by number
		guint i = 0;
		while ( i < AghEs && strcmp( AghEE[i], real_episode_name) )
			++i;
		printf( "%d, %d\n", i, AghEs);
		AghEi = i;
	}
	AghJ = _j;

	switch ( event->button ) {
	case 1:
		if ( event->state & GDK_CONTROL_MASK ) {  // Ctl+left click
			if ( clicked_episode != -1 && agh_prepare_scoring_facility() ) {
				// if ( __single_channel_wScoringFacility_height )
				// 	gtk_window_resize( GTK_WINDOW (wScoringFacility), 1200,
				//			   __single_channel_wScoringFacility_height);
				g_string_printf( window_title, "Scoring: %s\342\200\231s %s",
						 _j->name, AghE);
				gtk_window_set_title( GTK_WINDOW (wScoringFacility),
						      window_title->str);
				gtk_widget_show_all( wScoringFacility);

				J_paintable = J;
				__paint_one_subject_episodes();
				gtk_widget_queue_draw( J->da);
			}
		}
	    break;
	case 2:
	case 3:
		if ( clicked_episode != -1 ) {
			TEDFRef edfref;
			agh_edf_find_by_jdeh( _j->name, AghD, AghEE[clicked_episode], AghT,
					      &edfref);
			char *tmp;
			const struct SEDFFile *edfstruct = agh_edf_get_info_from_sourceref( edfref, &tmp);
			gtk_text_buffer_set_text( textbuf2, tmp, -1);
			free( tmp);
			g_string_printf( window_title, "%s header",
					 edfstruct->filename);
			gtk_window_set_title( GTK_WINDOW (wEDFFileDetails),
					      window_title->str);
			gtk_widget_show_all( wEDFFileDetails);
		}
	    break;
	}

	return TRUE;
}




gboolean
daSubjectTimeline_scroll_event_cb( GtkWidget *wid, GdkEventScroll *event, gpointer userdata)
{
	switch ( event->direction ) {
	case GDK_SCROLL_DOWN:
		if ( event->state & GDK_CONTROL_MASK ) {
			AghPPuV2 /= 1.3;
			gtk_widget_queue_draw( cMeasurements);
			return TRUE;
		}
	    break;
	case GDK_SCROLL_UP:
		if ( event->state & GDK_CONTROL_MASK ) {
			AghPPuV2 *= 1.3;
			gtk_widget_queue_draw( cMeasurements);
			return TRUE;
		}
	    break;
	default:
	    break;
	}

	return FALSE;
}








void
bMsmtDetails_toggled_cb( GtkToggleButton *_,
			 gpointer         user_data)
{
	gint is_on = gtk_toggle_tool_button_get_active( GTK_TOGGLE_TOOL_BUTTON (bMsmtDetails));
	AghMsmtViewDrawPowerSolid = !is_on;
	AghMsmtViewDrawDetails    =  is_on;

	gtk_widget_queue_draw( cMeasurements);
}

/*
static guint __single_channel_wScoringFacility_height = 0;

gboolean
wScoringFacility_configure_event_cb( GtkWidget *widget,
				     GdkEventConfigure *event,
				     gpointer unused)
{
	if ( __single_channel_wScoringFacility_height == 0 )
		__single_channel_wScoringFacility_height = event->height;
	return FALSE;
}

*/


// EOF
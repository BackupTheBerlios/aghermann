// ;-*-C++-*-
/*
 *       File name:  ui/expdesign_cb.cc
 *         Project:  Aghermann
 *          Author:  Andrei Zavada <johnhommer@gmail.com>
 * Initial version:  2011-06-29
 *
 *         Purpose:  SExpDesignUI widget callbacks
 *
 *         License:  GPL
 */

#include "misc.hh"
#include "expdesign.hh"
#include "scoring-facility.hh"


using namespace aghui;

extern "C" {

	void
	bExpChange_clicked_cb( GtkButton *button, gpointer userdata)
	{
		auto& ED = *(SExpDesignUI*)userdata;
		gtk_window_get_position( ED.wMainWindow, &ED.geometry.x, &ED.geometry.y);
		gtk_window_get_size( ED.wMainWindow, &ED.geometry.w, &ED.geometry.h);

		gtk_widget_show( (GtkWidget*)ED.wExpDesignChooser);
		gtk_widget_hide( (GtkWidget*)ED.wMainWindow);
		// if ( gtk_widget_get_visible( (GtkWidget*)wScoringFacility) )
		// 	gtk_widget_hide( (GtkWidget*)wScoringFacility);
		// better make sure bExpChange is greyed out on opening any child windows
	}



	gboolean
	wMainWindow_configure_event_cb( GtkWidget *wid, GdkEvent *event, gpointer userdata)
	{
		auto EDp = (SExpDesignUI*)userdata;
		if ( event->type == GDK_CONFIGURE ) {
			EDp->geometry.x = ((GdkEventConfigure*)event) -> x;
			EDp->geometry.y = ((GdkEventConfigure*)event) -> y;
			EDp->geometry.w = ((GdkEventConfigure*)event) -> width;
			EDp->geometry.h = ((GdkEventConfigure*)event) -> height;
		}
		return FALSE; // whatever
	}

	gboolean
	wMainWindow_delete_event_cb( GtkWidget *wid, GdkEvent *event, gpointer userdata)
	{
		auto& ED = *(SExpDesignUI*)userdata;
		if ( ED.nodestroy_by_cb )
			return TRUE;

		// check if any facilities are open, and prompt?
		// let the destructor handle all the saving

		ED.save_settings();
		ED.finalize_ui = false;
		// delete EDp; // no
		gtk_main_quit();

		return TRUE; // whatever
	}


      // tab switch
	void
	tTaskSelector_switch_page_cb( GtkNotebook     *notebook,
				      gpointer	       unused,
				      guint            page_num,
				      gpointer         userdata)
	{
		auto& ED = *(SExpDesignUI*)userdata;
		if ( page_num == 1 ) {
			//ED.populate( false);
			snprintf_buf( "Session: <b>%s</b>", ED.AghD());
			gtk_label_set_markup( ED.lSimulationsSession, __buf__);
			snprintf_buf( "Channel: <b>%s</b>", ED.AghT());
			gtk_label_set_markup( ED.lSimulationsChannel, __buf__);
			gtk_widget_set_sensitive( (GtkWidget*)ED.bExpChange, FALSE);
			ED.populate_2();
		} else if ( page_num == 0 ) {
			// ED.ED->remove_untried_modruns(); // done in populate_2
			// ED.populate( false);
			gtk_widget_set_sensitive( (GtkWidget*)ED.bExpChange, TRUE);
		}
	}


      // -------- colours
	void
	bColourX_color_set_cb( GtkColorButton *widget,
			       gpointer        userdata)
	{
		auto& mc = *(SManagedColor*)userdata;
		mc.acquire();
	}
}

// eof

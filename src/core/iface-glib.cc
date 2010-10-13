// ;-*-C++-*- *  Time-stamp: "2010-10-04 02:44:44 hmmr"
/*
 *       File name:  core/iface-expdesign-glib.cc
 *         Project:  Aghermann
 *          Author:  Andrei Zavada (johnhommer@gmail.com)
 * Initial version:  2010-09-26
 *
 *         Purpose:  some iface-expdesign functions adapted for use with glib GArray, GString
 *
 *         License:  GPL
 */


#include "iface-glib.h"

#include "edf.hh"
#include "primaries.hh"
#include "model.hh"



#define __R (static_cast<CSimulation*>(Ri))



size_t
agh_edf_get_scores_as_garray( TEDFRef _F,
			      GArray *scores, size_t *pagesize_p)
{
	CEDFFile& F = *static_cast<CEDFFile*>(_F);

	if ( scores->len < F.CHypnogram::length() )
		g_array_set_size( scores, F.CHypnogram::length()); // why does this not preserve previous content?

	for ( size_t p = 0; p < F.CHypnogram::length(); ++p )
		Ai (scores, gchar, p) = F.nth_page(p).p2score();

	if ( pagesize_p)
		*pagesize_p = F.pagesize();

	return F.CHypnogram::length();
}





int
agh_edf_put_scores_as_garray( TEDFRef _F,
			      GArray *scores)
{
	CEDFFile& F = *static_cast<CEDFFile*>(_F);

	for ( size_t p = 0; p < F.CHypnogram::length() && p < scores->len; ++p )
		if ( Ai (scores, gchar, p) == AghScoreCodes[AGH_SCORE_NREM1] )
			F.nth_page(p).NREM =  .25;
		else if ( Ai (scores, gchar, p) == AghScoreCodes[AGH_SCORE_NREM2] )
			F.nth_page(p).NREM =  .50;
		else if ( Ai (scores, gchar, p) == AghScoreCodes[AGH_SCORE_NREM3] )
			F.nth_page(p).NREM =  .75;
		else if ( Ai (scores, gchar, p) == AghScoreCodes[AGH_SCORE_NREM4] )
			F.nth_page(p).NREM = 1.;
		else if ( Ai (scores, gchar, p) == AghScoreCodes[AGH_SCORE_REM] )
			F.nth_page(p).REM  = 1.;
		else if ( Ai (scores, gchar, p) == AghScoreCodes[AGH_SCORE_WAKE] )
			F.nth_page(p).Wake = 1.;
		else if ( Ai (scores, gchar, p) == AghScoreCodes[AGH_SCORE_MVT] )
			F.nth_page(p).Wake = AGH_MVT_WAKE_VALUE; // .01 is specially reserved for MVT
	return 0;
}






size_t
agh_msmt_get_track_as_garray( TRecRef ref,
			      GArray *track)
{
	CRecording& K = *static_cast<CRecording*>(ref);

	size_t length = K.length_in_seconds();
	g_array_set_size( track, length);
	memcpy( track->data, K.artifacts(), length * sizeof(char));

	return length;
}


void
agh_msmt_put_track_as_garray( TRecRef ref,
			      GArray *track)
{
	CRecording& K = *static_cast<CRecording*>(ref);

	K.import_artifacts( track->data);
}






size_t
agh_msmt_get_power_spectrum_as_double_garray( TRecRef ref, size_t p,
					      GArray *out)
{
	CRecording& K = *static_cast<CRecording*>(ref);

	valarray<double> power_acc = K.power_spectrum(p);
	g_array_set_size( out, K.n_bins());
	memcpy( out->data, &power_acc[0], K.n_bins() * sizeof(double));

	return K.n_bins();

}

size_t
agh_msmt_get_power_spectrum_as_float_garray( TRecRef ref, size_t p,
					     GArray *out)
{
	CRecording& K = *static_cast<CRecording*>(ref);

	valarray<float> power_acc = K.power_spectrumf(p);
	g_array_set_size( out, K.n_bins());
	memcpy( out->data, &power_acc[0], K.n_bins() * sizeof(float));

	return K.n_bins();

}




size_t
agh_msmt_get_power_course_in_range_as_double_garray( TRecRef ref,
						     float from, float upto,
						     GArray *out)
{
	CRecording& K = *static_cast<CRecording*>(ref);

	valarray<double> power_acc = K.power_course( from, upto);
	size_t n_pages = power_acc.size();
	g_array_set_size( out, n_pages);
	memcpy( out->data, &power_acc[0], n_pages * sizeof(double));

	return n_pages;
}



size_t
agh_msmt_get_power_course_in_range_as_float_garray( TRecRef ref,
						    float from, float upto,
						    GArray *out)
{
	CRecording& K = *static_cast<CRecording*>(ref);

	valarray<float> power_acc = K.power_coursef( from, upto);
	size_t n_pages = power_acc.size();
	g_array_set_size( out, n_pages);
	memcpy( out->data, &power_acc[0], n_pages * sizeof(float));

	return n_pages;
}





void
agh_modelrun_get_all_courses_as_double_garray( TModelRef Ri,
					       GArray *SWA_out, GArray *S_out, GArray *SWAsim_out, GArray *scores_out)
{
	size_t p;
	g_array_set_size (SWA_out,    __R->timeline.size());
	g_array_set_size (S_out,      __R->timeline.size());
	g_array_set_size (SWAsim_out, __R->timeline.size());
	g_array_set_size (scores_out, __R->timeline.size());
	for ( p = 0; p < __R->timeline.size(); ++p ) {
		SPageSimulated &P = __R->timeline[p];
		Ai (SWA_out,    double, p) = P.SWA;
		Ai (S_out,      double, p) = P.S;
		Ai (SWAsim_out, double, p) = P.SWA_sim;
		Ai (scores_out, char,   p) = P.p2score();
	}
}

void
agh_modelrun_get_mutable_courses_as_double_garray( TModelRef Ri,
						   GArray *S_out, GArray *SWAsim_out)
{
	size_t p;
	g_array_set_size (S_out,      __R->timeline.size());
	g_array_set_size (SWAsim_out, __R->timeline.size());
	for ( p = 0; p < __R->timeline.size(); ++p ) {
		SPageSimulated &P = __R->timeline[p];
		Ai (S_out,      double, p) = P.S;
		Ai (SWAsim_out, double, p) = P.SWA_sim;
	}
}



// EOF
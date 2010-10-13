// ;-*-C++-*- *  Time-stamp: "2010-09-26 15:59:14 hmmr"
/*
 *       File name:  core/iface-simulations.cc
 *         Project:  Aghermann
 *          Author:  Andrei Zavada (johnhommer@gmail.com)
 * Initial version:  2010-05-01
 *
 *         Purpose:  
 *
 *         License:  GPL
 */



#include <cassert>
#include <sys/stat.h>
#include <gtk/gtk.h>
#include "iface.h"
#include "../core/primaries.hh"

extern CExpDesign *AghCC;


#ifdef __cplusplus
extern "C" {
#endif

#define __R (static_cast<CSimulation*>(Ri))


static list<CSimulation>::iterator __agh_modelrun_iter;

TModelRef
agh_modelrun_find_first()
{
	return AghCC->simulations.size() ? &*(__agh_modelrun_iter = AghCC->simulations.begin()) : (TModelRef)0;
}

TModelRef
agh_modelrun_find_next()
{
	if ( __agh_modelrun_iter == AghCC->simulations.end() )
		return NULL;
	else
		return &*(++__agh_modelrun_iter);
}



const char*
agh_modelrun_get_subject( TModelRef Ri)
{
	return __R->subject;
}
const char*
agh_modelrun_get_session( TModelRef Ri)
{
	return __R->session;
}
const char*
agh_modelrun_get_channel( TModelRef Ri)
{
	return __R->channel;
}


int
agh_modelrun_setup( const char *j_name, const char *d_name, const char *h_name,
		    float from, float upto,
		    TModelRef *Rp)
{
//	CSimulation *&Ri = (CSimulation*&)*Rp;
	return AghCC -> setup_modrun( j_name, d_name, h_name,
				      from, upto,
				      (CSimulation*&)*Rp);
}


int
agh_modelrun_run( TModelRef Ri)
{
	return __R->watch_simplex_move();
}


void
agh_modelrun_snapshot( TModelRef Ri)
{
	__R->snapshot();
}


int
agh_modelrun_exist( const char *j_name, const char *d_name, const char *h_name,
		    float from, float upto)
{
	return AghCC->have_modrun( j_name, d_name, h_name, from, upto);
}



TModelRef
agh_modelrun_find_by_jdhq( const char *j_name, const char *d_name, const char *h_name,
			   float from, float upto)
{
	try {
		CSimulation& R = AghCC -> modrun_by_jdhq( j_name, d_name, h_name, from, upto);
		return static_cast<TModelRef>(&R);
	} catch (const char *ex) {
		fprintf( stderr, "agh_modelrun_find_by_jdhq: no simulation for j=%s, d=%s, h=%s in freqq %g-%g\n",
			 j_name, d_name, h_name, from, upto);
		return NULL;
	}
}


void
agh_modelrun_reset( TModelRef Ri)
{
	AghCC -> reset_modrun( *__R);
}




size_t
agh_modelrun_get_n_episodes( TModelRef Ri)
{
	return __R->mm_bounds.size();
}




size_t
agh_modelrun_get_nth_episode_start_p( TModelRef Ri, size_t e)
{
	return __R->mm_bounds[e].first;
}

size_t
agh_modelrun_get_nth_episode_end_p( TModelRef Ri, size_t e)
{
	return __R->mm_bounds[e].second;
}

size_t
agh_modelrun_get_pagesize( TModelRef Ri)
{
	return __R->pagesize();
}



void
agh_modelrun_get_all_courses_as_double( TModelRef Ri,
					double **SWA_out, double **S_out, double **SWAsim_out,
					char **scores_out)
{
	size_t p;
	*SWA_out    = (double*)malloc( __R->timeline.size() * sizeof(double));
	*S_out      = (double*)malloc( __R->timeline.size() * sizeof(double));
	*SWAsim_out = (double*)malloc( __R->timeline.size() * sizeof(double));
	*scores_out = (char*)  malloc( __R->timeline.size() * sizeof(char));
	for ( p = 0; p < __R->timeline.size(); ++p ) {
		SPageSimulated &P = __R->timeline[p];
		(*SWA_out)   [p] = P.SWA;
		(*S_out)     [p] = P.S;
		(*SWAsim_out)[p] = P.SWA_sim;
		(*scores_out)[p] = P.p2score();
	}
}

void
agh_modelrun_get_mutable_courses_as_double( TModelRef Ri,
					    double **S_out, double **SWAsim_out)
{
	size_t p;
	*S_out      = (double*)malloc( __R->timeline.size() * sizeof(double));
	*SWAsim_out = (double*)malloc( __R->timeline.size() * sizeof(double));
	for ( p = 0; p < __R->timeline.size(); ++p ) {
		SPageSimulated &P = __R->timeline[p];
		(*S_out)     [p] = P.S;
		(*SWAsim_out)[p] = P.SWA_sim;
	}
}




void
agh_modelrun_save( TModelRef Ri)
{
	__R->save( AghCC -> make_fname_simulation( __R->subject, __R->session, __R->channel,
						   __R->freq_from, __R->freq_upto).c_str());
}





void
agh_modelrun_get_tunables( TModelRef Ri, struct SConsumerTunableSet *t_set)
{
	t_set->n_tunables = __R->cur_tset.P.size();
	t_set->tunables = (double*)realloc( t_set->tunables, t_set->n_tunables * sizeof(double));
	memcpy( t_set->tunables, &__R->cur_tset.P[0], t_set->n_tunables * sizeof(double));
}
void
agh_modelrun_put_tunables( TModelRef Ri, const struct SConsumerTunableSet *t_set)
{
	assert (t_set->n_tunables == __R->cur_tset.P.size());
	memcpy( &__R->cur_tset.P[0], t_set->tunables, t_set->n_tunables * sizeof(double));
}



void
agh_modelrun_get_ctlparams( TModelRef Ri, struct SConsumerCtlParams *ctl_params)
{
	memcpy( &ctl_params->siman_params, &__R->siman_params, sizeof(gsl_siman_params_t));
	ctl_params->DBAmendment1 = __R->DBAmendment1;
	ctl_params->DBAmendment2 = __R->DBAmendment2;
	ctl_params->AZAmendment  = __R->AZAmendment;
	ctl_params->ScoreMVTAsWake = __R->ScoreMVTAsWake;
	ctl_params->ScoreUnscoredAsWake = __R->ScoreUnscoredAsWake;
}
void
agh_modelrun_put_ctlparams( TModelRef Ri, const struct SConsumerCtlParams *ctl_params)
{
	memcpy( &__R->siman_params, &ctl_params->siman_params, sizeof(gsl_siman_params_t));
	__R->DBAmendment1 = ctl_params->DBAmendment1;
	__R->DBAmendment2 = ctl_params->DBAmendment2;
	__R->AZAmendment  = ctl_params->AZAmendment;
	__R->ScoreMVTAsWake = ctl_params->ScoreMVTAsWake;
	__R->ScoreUnscoredAsWake = ctl_params->ScoreUnscoredAsWake;
}




int
agh_modelrun_tsv_export_one( TModelRef Ri, const char *fname)
{
	FILE *f = fopen( fname, "w");
	if ( !f )
		return -1;

	fprintf( f, "#Subject: %s;  Session: %s;  Channel: %s;  Range: %g-%g\n#",
		 __R->subject, __R->session, __R->channel,
		 __R->freq_from, __R->freq_upto);
	size_t t;
	for ( t = 0; t < _gc_+1; ++t )
		fprintf( f, "\t%s", __AGHTT[t].name);
	for ( ; t < __R->cur_tset.P.size(); ++t )
		fprintf( f, "\tgc%zu", t - _gc_);
	fprintf( f, "\n");

	for ( t = 0; t < __R->cur_tset.P.size(); ++t )
		fprintf( f, "\t%g", __R->cur_tset.P[t]);

	fclose( f);

	return 0;
}

int
agh_modelrun_tsv_export_all( const char* fname)
{
	FILE *f = fopen( fname, "w");
	if ( !f )
		return -1;

	for ( auto Ri = AghCC->simulations.begin(); Ri != AghCC->simulations.end(); ++Ri ) {
		fprintf( f, "#Subject: %s;  Session: %s;  Channel: %s;  Range: %g-%g\n#",
			 Ri->subject, Ri->session, Ri->channel,
			 Ri->freq_from, Ri->freq_upto);
		size_t t;
		for ( t = 0; t < _gc_+1; ++t )
			fprintf( f, "\t%s", __AGHTT[t].name);
		for ( ; t < Ri->cur_tset.P.size(); ++t )
			fprintf( f, "\tgc%zu", t - _gc_);
		fprintf( f, "\n");

		for ( t = 0; t < Ri->cur_tset.P.size(); ++t )
			fprintf( f, "\t%g", Ri->cur_tset.P[t]);

	}

	fclose( f);

	return 0;
}

#undef __R






void
agh_ctlparams0_get( struct SConsumerCtlParams *ctl_params)
{
	memcpy( &ctl_params->siman_params, &AghCC->control_params.siman_params, sizeof(gsl_siman_params_t));
	ctl_params->DBAmendment1 = AghCC->control_params.DBAmendment1;
	ctl_params->DBAmendment2 = AghCC->control_params.DBAmendment2;
	ctl_params->AZAmendment  = AghCC->control_params.AZAmendment;
	ctl_params->ScoreMVTAsWake = AghCC->control_params.ScoreMVTAsWake;
	ctl_params->ScoreUnscoredAsWake = AghCC->control_params.ScoreUnscoredAsWake;
}
void
agh_ctlparams0_put( const struct SConsumerCtlParams *ctl_params)
{
	memcpy( &AghCC->control_params.siman_params, &ctl_params->siman_params, sizeof(gsl_siman_params_t));
	AghCC->control_params.DBAmendment1 = ctl_params->DBAmendment1;
	AghCC->control_params.DBAmendment2 = ctl_params->DBAmendment2;
	AghCC->control_params.AZAmendment  = ctl_params->AZAmendment;
	AghCC->control_params.ScoreMVTAsWake = ctl_params->ScoreMVTAsWake;
	AghCC->control_params.ScoreUnscoredAsWake = ctl_params->ScoreUnscoredAsWake;
}




void
agh_modelrun_collect_from_tree( float from, float upto)
{
	AghCC -> collect_simulations_from_tree( from, upto);
}


#ifdef __cplusplus
}
#endif


// EOF